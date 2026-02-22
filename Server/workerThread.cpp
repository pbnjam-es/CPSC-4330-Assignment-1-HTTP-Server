#include <thread>
#include <iostream>
#include <unistd.h>
#include <fstream>
#include <filesystem>
#include <fcntl.h>
#include <vector>
#include <sys/stat.h>
#include <ctime>

namespace fs = std::filesystem;

#include "connectionQueue.hpp"
#include "workerThread.hpp"
#include "config.hpp"
#include "operatorThread.hpp"
#include "helpers.hpp"


void workerThread(ConnectionQueue& connection_queue, std::vector<VirtualHost>& vhosts) {
    while (server_running) {
        Connection conn = {};
        if (!server_running) {
            break;
        }
        bool pop = connection_queue.pop(conn);
        if (!pop) {
            continue;
        }
        // std::cout << "Popped connection fd=" << conn.fd 
        //   << " ip=" << conn.client_ip 
        //   << " port=" << conn.client_port << "\n";
        if (conn.fd < 0) {
            std::cerr << "ERROR: popped connection with invalid fd\n";
            continue;
        }
        while (true) {
            conn.readBuffer.clear();
            std::cout << "[Worker] Processing connection from "
            << conn.client_ip << ":" << conn.client_port << "\n";
            

            if (!readWithNonBlocking(conn, conn.readBuffer, 3)) {
                std::cout << "Connection timed out " << conn.client_ip << "\n";
                close(conn.fd);
                break;
            }
            parseHttpRequest(conn);
            if (conn.request.url == "/load") {
                handleLoadRequestThreaded(conn, connection_queue, 50);
                close(conn.fd);
                break;
            }
            VirtualHost* vh = resolveVhost(vhosts, conn.request);
            // sendBasicResponse(conn);
            std::string path = buildPath(*vh, conn.request);

            if (!checkAuth(conn, path)) {
                close(conn.fd);
                std::cout << "Incorrect Authentication" << "\n";
                break;
            }
            if (isExecutable(path)) {
                executeCgi(conn, path, *vh);
            }
            else {
                servePath(conn, path);
            }
            
            if (!conn.keepAlive) {
                close(conn.fd);
                break;
            }

            conn.readBuffer.clear();
            conn.request = {};
    
        }
    }

}


bool readWithNonBlocking(Connection &connection, std::string &buffer, int timeoutSeconds) {
    buffer.clear();
    char tmp[1024];
    connection.acceptedTime = std::chrono::steady_clock::now();
    while (true) {
        ssize_t n = read(connection.fd, tmp, sizeof(tmp));
        if (n > 0) {
            std::cout << "[Worker] Received " << n << " bytes\n";
            buffer.append(tmp, n);
            if (buffer.find("\r\n\r\n") != std::string::npos) {
                // check if POST with body
                auto pos = buffer.find("Content-Length:");
                if (pos != std::string::npos) {
                    int contentLength = std::stoi(buffer.substr(pos + 15));
                    auto headerEnd = buffer.find("\r\n\r\n");
                    if (buffer.size() >= headerEnd + 4 + contentLength) {
                        return true; // have all headers + body
                    }
                    // keep reading
                } else {
                    return true; // no body expected
                }
            }

        } else if (n == 0) {
            std::cout << "Client closed connection\n" << std::flush;
            // client closed connection
            return false;
        } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("read");
            return false;
        }

        // check timeout
        auto now = std::chrono::steady_clock::now();
        // std::cout << std::chrono::duration_cast<std::chrono::seconds>(now - connection.acceptedTime).count() << "\n" << std::flush;
        if (std::chrono::duration_cast<std::chrono::seconds>(now - connection.acceptedTime).count() >= timeoutSeconds) {
            return false; // timeout
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50)); // avoid busy loop
    }
}

void handleLoadRequestThreaded(Connection &conn, ConnectionQueue& connection_queue, size_t max) {
    std::string response;
    if (connection_queue.size() < max) {
        response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 2\r\n\r\nOK";
    }
    else {
        response =
        "HTTP/1.1 503 Service Unavaliable\r\n"
        "Content-Length: 9\r\n\r\nOverloaded";
    }
    write(conn.fd, response.c_str(), response.size());
}

void handleLoadRequestSelected(Connection &conn, std::vector<Connection>& connection_queue, size_t max) {
    std::string response;
    if (connection_queue.size() < max) {
        response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 2\r\n\r\nOK";
    }
    else {
        response =
        "HTTP/1.1 503 Service Unavaliable\r\n"
        "Content-Length: 9\r\n\r\nOverloaded";
    }
    write(conn.fd, response.c_str(), response.size());
}


void parseHttpRequest(Connection &conn) {
    conn.request.headers.clear();
    conn.request.body.clear();

    std::istringstream stream(conn.readBuffer);
    std::string line;
    if (!std::getline(stream, line)) return;
    line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
    std::istringstream top_line(line);
    top_line >> conn.request.method >> conn.request.url >> conn.request.version;

    // Extract query string
    auto qmark = conn.request.url.find('?');
    if (qmark != std::string::npos) {
        conn.request.queryString = conn.request.url.substr(qmark+1);
        conn.request.url = conn.request.url.substr(0, qmark);
    }
    std::cout << conn.request.url << "\n" << std::flush;
    while (std::getline(stream, line)) {
        line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
        if (line.empty()) break; 
        auto colon = line.find(':');
        if (colon != std::string::npos && colon != 0) {
            std::string key = line.substr(0, colon);
            std::string value = line.substr(colon + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            conn.request.headers[key] = value;
            // conn.request.headers["host"] = "localhost";
        }
    }

    if (conn.request.headers.find("Connection") != conn.request.headers.end()) {
        if (conn.request.headers["Connection"] == "close") {
            conn.keepAlive = false;
        }
    }

    // Body (for POST)
    if (conn.request.method == "POST") {
        auto it = conn.request.headers.find("Content-Length");
        if (it != conn.request.headers.end()) {
            int contentLength = std::stoi(it->second);
            auto pos = conn.readBuffer.find("\r\n\r\n");
            if (pos != std::string::npos) {
                conn.request.body = conn.readBuffer.substr(pos + 4, contentLength);
            }
        }
    }

}

std::string buildPath(VirtualHost& vhost, HttpRequest& httpRequest) {
    std::string sanitized_url = sanitizeUrl(httpRequest);
    if (sanitized_url.empty()) return "";
    std::string full_path = vhost.documentRoot + sanitized_url;
    std::error_code ec;
    fs::path absolute_url = fs::weakly_canonical(full_path, ec);
    fs::path absolute_root = fs::weakly_canonical(vhost.documentRoot, ec);

    auto rel = fs::relative(absolute_url, absolute_root, ec);
    if (!rel.empty() && *rel.begin() == "..") {
        return ""; // traversal detected
    }

    return full_path;
}

void send304(Connection &conn, struct stat &st) {
    std::string resp = "HTTP/1.1 304 Not Modified\r\n";
    resp += "Date: " + httpDate(std::time(nullptr)) + "\r\n";
    resp += "Last-Modified: " + httpDate(st.st_mtime) + "\r\n";
    resp += "Connection: " + std::string(conn.keepAlive ? "keep-alive" : "close") + "\r\n";
    resp += "\r\n";
    write(conn.fd, resp.c_str(), resp.size());
}

void send400(Connection &conn) {
    std::string resp = "HTTP/1.1 400 Bad Request\r\n";
    resp += "Date: " + httpDate(std::time(nullptr)) + "\r\n";
    resp += "Content-Length: 0\r\n";
    resp += "Connection: close\r\n";
    resp += "\r\n";    
    write(conn.fd, resp.c_str(), resp.size());
}
void send401(Connection &conn, HtAccess htaccess) {
    std::string resp = "HTTP/1.1 401 Unauthorized\r\n";
    resp += "Date: " + httpDate(std::time(nullptr)) + "\r\n";
    resp += "WWW-Authenticate: Basic realm=\"" + htaccess.authName + "\"\r\n";
    resp += "Content-Length: 0\r\n";
    resp += "Connection: " + std::string(conn.keepAlive ? "keep-alive" : "close") + "\r\n";
    resp += "\r\n";
    write(conn.fd, resp.c_str(), resp.size());
}

void send403(Connection &conn) {
    std::string resp = "HTTP/1.1 403 Forbidden\r\nDate: " + 
    httpDate(std::time(nullptr)) + 
    "\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    write(conn.fd, resp.c_str(), resp.size());
}


void send404(Connection &conn) {
    std::string resp =
        "HTTP/1.1 404 Not Found\r\n" 
        "Date: " + httpDate(std::time(nullptr)) + "\r\n"
        "Content-Length: 0\r\n"
        "Connection: " + (conn.keepAlive ? "keep-alive" : "close") + "\r\n";

    resp += "\r\n";
    write(conn.fd, resp.c_str(), resp.size());
}

void send406(Connection &conn) {
    std::string resp = "HTTP/1.1 406 Not Acceptable\r\n";
    resp += "Date: " + httpDate(std::time(nullptr)) + "\r\n";
    resp += "Content-Length: 0\r\n";
    resp += "Connection: " + std::string(conn.keepAlive ? "keep-alive" : "close") + "\r\n";
    resp += "\r\n";
    write(conn.fd, resp.c_str(), resp.size());
    return;
}

bool checkAuth(Connection &conn, std::string &path) {
    std::string dir = path.substr(0, path.find_last_of("/"));
    HtAccess htaccess; 
    if (!parseHtAccess(dir, htaccess)) {
        // No .htaccess file.
        return true;
    }
    auto it = conn.request.headers.find("Authorization");
    if (it == conn.request.headers.end()) {
        send401(conn, htaccess);
        return false;
    }

    std::string authVal = it->second;
    if (authVal.substr(0, 6) != "Basic ") {
        send401(conn, htaccess);
        return false;
    }

    std::string provided = authVal.substr(6);
    std::string expectedUser = base64Decode(htaccess.user);
    std::string expectedPass = base64Decode(htaccess.password);
    std::string providedDecoded = base64Decode(provided);

    std::string expected = expectedUser + ":" + expectedPass;

    if (providedDecoded != expected) {
        send401(conn, htaccess);
        return false;
    }
    return true;
}

void servePath(Connection &conn, std::string path) {
    if (path.empty()) {
        send400(conn);
        conn.keepAlive = false;
        return;     
    }
    
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        send404(conn);
        return;
    }
    std::string file_type = fileType(path);
    if (!acceptContentType(conn, file_type)) {
        send406(conn);
        return;
    }

    struct stat st;
    std::time_t lastMod = 0;
    if (stat(path.c_str(), &st) == 0) {
        lastMod = st.st_mtime;
    }

    auto ims = conn.request.headers.find("If-Modified-Since");
    if (ims != conn.request.headers.end()) {
        struct tm tm = {};
        strptime(ims->second.c_str(), "%a, %d %b %Y %H:%M:%S GMT", &tm);
        if (st.st_mtime <= timegm(&tm)) {
            send304(conn, st);
            return;
        }
    }

    std::vector<char> data = {
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()
    };

    std::ostringstream response;
    response << "HTTP/1.1 200 OK\r\n" 
        << "Date: " << httpDate(std::time(nullptr)) << "\r\n"
        << "Server: Server\r\n" 
        << "Last-Modified: " << httpDate(lastMod) << "\r\n"
        << "Content-Type: " << fileType(path) << "\r\n"
        << "Content-Length: " << data.size() << "\r\n"
        << "Connection: " << (conn.keepAlive ? "keep-alive" : "close") << "\r\n"
        << "\r\n";
    
    write(conn.fd, response.str().c_str(), response.str().size());
    write(conn.fd, data.data(), data.size());
    
}

void executeCgi(Connection& conn, std::string& path, VirtualHost& vhost) {
    if (access(path.c_str(), F_OK) != 0) {
        send404(conn);
        return;
    }
    if (access(path.c_str(), X_OK) != 0) {
        send403(conn);
        return;
    }
    int stdinPipe[2];
    int stdoutPipe[2];
    pipe(stdinPipe);
    pipe(stdoutPipe);

    pid_t pid = fork();
    if (pid == 0) {
        close(conn.fd);
        dup2(stdinPipe[0], STDIN_FILENO);
        dup2(stdoutPipe[1], STDOUT_FILENO);
        close(stdinPipe[1]);
        close(stdoutPipe[0]);

        std::vector<std::string> env = build_envp(conn, vhost);
        std::vector<char*> envp;
        for (auto &parameter : env) {
            envp.push_back(const_cast<char*>(parameter.c_str()));
        }
        envp.push_back(nullptr);
        char* argv[] = {const_cast<char*>(path.c_str()), nullptr};
        execve(path.c_str(), argv, envp.data());
        exit(1);

    }
    else {
        close(stdinPipe[0]);
        close(stdoutPipe[1]);
        if (conn.request.method == "POST") {
            write(stdinPipe[1], conn.request.body.c_str(), conn.request.body.size());
        }
        close(stdinPipe[1]);

        std::string prelude = "HTTP/1.1 200 OK\r\n";
        prelude += "Date: " + httpDate(std::time(nullptr)) + "\r\n";
        prelude += "Server: Server\r\n";
        // prelude += "Content-Type: " + fileType(path) + "\r\n";
        prelude += "Transfer-Encoding: chunked\r\n";
        prelude += "Connection: " + std::string(conn.keepAlive ? "keep-alive" : "close") + "\r\n";
        prelude += "\r\n";
        write(conn.fd, prelude.c_str(), prelude.size());

        char buf[4096];
        ssize_t n;
        while ((n = read(stdoutPipe[0], buf, sizeof(buf))) > 0) {
            std::ostringstream chunk;
            chunk << std::hex << n << "\r\n";
            // std::cerr << "chunk size: " << n << " hex: " << std::hex << n << "\n";
            write(conn.fd, chunk.str().c_str(), chunk.str().size());
            write(conn.fd, buf, n);
            write(conn.fd, "\r\n", 2);
        }
        // std::cerr << "sending terminating chunk\n";
        write(conn.fd, "0\r\n\r\n", 5);
        close(stdoutPipe[0]);
        
        int status;
        waitpid(pid, &status, 0);
        
    }
}