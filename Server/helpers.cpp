#include "helpers.hpp"
#include "config.hpp"
#include <filesystem>

namespace fs = std::filesystem;


std::string httpDate(std::time_t t) {
    char buf[128];
    struct tm* gmt = std::gmtime(&t);
    std::strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", gmt);
    return std::string(buf);
}

VirtualHost* resolveVhost(std::vector<VirtualHost>& vhosts, HttpRequest& httpRequest) {
    auto it = httpRequest.headers.find("Host");
    if (it == httpRequest.headers.end()) {
        return &vhosts[0];
    }
    std::string host = it -> second;
    auto colon = host.find(':');
    if (colon != std::string::npos) {
        host = host.substr(0, colon);
    }
    for (VirtualHost& virtualHost : vhosts) {
        if (virtualHost.serverName == host) {
            return &virtualHost;
        }
    }
    return &vhosts[0];
}

std::string resolveUserAgent(HttpRequest& httpRequest) {
    auto it = httpRequest.headers.find("User-Agent:");
    if (it == httpRequest.headers.end()) {
        return "Not Found";
    }
    std::string userAgent = it -> second;
    return userAgent;

}

std::string sanitizeUrl(HttpRequest& httpRequest) {
    std::string url = httpRequest.url;
    if (!url.empty() && url.back() == '\r') url.pop_back();

    if (url == "/" || url.empty()) {
        std::string userAgent = resolveUserAgent(httpRequest);
        if (userAgent.find("iPhone") != std::string::npos) {
            return "/index_m.html";
        }
        return "/index.html";
    }
    return url;
}

std::string fileType(std::string path) {
    if (path.ends_with(".html")) {
        return "text/html";
    }
    if (path.ends_with(".css")) {
        return "text/css";
    }
    if (path.ends_with(".js")) {
        return "application/javascript";
    }
    if (path.ends_with(".png")) {
        return "image/png";
    }
    if (path.ends_with(".jpg")) {
        return "image/jpeg";
    }
    if (path.ends_with(".txt")) {
        return "text/plain";
    }
    return "application/octet-stream";
}



bool isExecutable(std::string& path) {
    std::error_code ec;
    fs::file_status status = fs::status(path, ec);

    if (ec) {
        std::cerr << "Error getting file status: " << ec.message() << std::endl;
        return false;
    }

    if (fs::is_regular_file(status) && (status.permissions() & fs::perms::owner_exec) != fs::perms::none) {
        return true;
    }
    return false;
}

std::vector<std::string> build_envp(Connection& conn, VirtualHost& vhost) {
    std::vector<std::string> envp;
    envp.push_back("REQUEST_METHOD=" + conn.request.method);
    envp.push_back("QUERY_STRING=" + conn.request.queryString);
    auto it = conn.request.headers.find("Content-Type");
    envp.push_back("CONTENT_TYPE=" + (it != conn.request.headers.end() ? it -> second : ""));
    envp.push_back("CONTENT_LENGTH=" + std::to_string(conn.request.body.size()));
    envp.push_back("SERVER_NAME=" + vhost.serverName);
    envp.push_back("SERVER_PORT=" + std::to_string(vhost.port));
    envp.push_back("REMOTE_PORT=" + std::to_string(conn.client_port));
    envp.push_back("REMOTE_ADDR=" + conn.client_ip);
    envp.push_back("SCRIPT_NAME=" + conn.request.url);
    envp.push_back("PATH_INFO=" + conn.request.url);
    envp.push_back("SERVER_PROTOCOL=HTTP/1.1");
    envp.push_back("SERVER_SOFTWARE=Server");

    return envp;
}


std::string base64Decode(const std::string& in) {
    static const std::string table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    int val = 0, valb = -8;
    for (unsigned char c : in) {
        if (c == '=') break;
        auto pos = table.find(c);
        if (pos == std::string::npos) continue;
        val = (val << 6) + pos;
        valb += 6;
        if (valb >= 0) {
            out.push_back((val >> valb) & 0xFF);
            valb -= 8;
        }
    }
    return out;
}

bool acceptContentType(Connection &conn, std::string &contentType) {
    auto it = conn.request.headers.find("Accept");
    if (it == conn.request.headers.end()) {
        return true;
    }
    std::string type = it -> second;
    if (type == "*/*") {
        return true;
    }
    std::string majorType = contentType.substr(0, contentType.find('/'));
    if (type.find(majorType + "/*") != std::string::npos) return true;

    return type.find(contentType) != std::string::npos;
}