#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdexcept>
#include <cstring>
#include <iostream>
#include <arpa/inet.h>
#include <fcntl.h>

#include "connection.hpp"
#include "config.hpp"
#include "connectionQueue.hpp"
#include "operatorThread.hpp"
#include "selectLoop.hpp"

// extern std::atomic<bool> server_running;

int startListeningSocket(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) throw std::runtime_error("socket failed");

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(fd, (sockaddr*)&addr, sizeof(addr)) < 0)
        throw std::runtime_error("bind failed");

    if (listen(fd, SOMAXCONN) < 0) 
        throw std::runtime_error("listen failed");

    return fd;
}


void setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl F_GETFL");
        return;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl F_SETFL O_NONBLOCK");
    }
}


void runAcceptLoopThreaded(int listen_fd, ServerConfig &serverConfig, ConnectionQueue &connectionQueue) {

    std::cout << "Listening on port " << serverConfig.listen_port << "\n";
    while (server_running) {
        
        sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client = accept(listen_fd, (sockaddr*)&client_addr, &client_len);
        if (client >= 0) {
            setNonBlocking(client);

            Connection connection = {};
            connection.fd = client;
            connection.client_ip = inet_ntoa(client_addr.sin_addr);
            connection.client_port = ntohs(client_addr.sin_port);
            connection.acceptedTime = std::chrono::steady_clock::now();
            std::cout << "[Accept Loop Threaded] Accepted connection from "
            << connection.client_ip   // converts s_addr to "x.x.x.x"
            << ":" << connection.client_port << "\n";
            // std::cout << connection.keepAlive << " keepalive test accept\n" << std::flush;
            // connectionQueue.push(std::move(connection));
            connectionQueue.push(connection);
            // close(client); // Phase 0: do nothing
        }
        else {
            if (!server_running) {
                break;
            }
            perror("accept error");
            continue;
        }
    }
}

void runAcceptLoopSelected(int listen_fd, ServerConfig &serverConfig, std::vector<SelectLoop> &loops) {
    std::cout << "Listening on port " << serverConfig.listen_port << "\n";

    int count = 0;
    while (server_running) {
        
        sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client = accept(listen_fd, (sockaddr*)&client_addr, &client_len);
        if (client >= 0) {
            setNonBlocking(client);

            Connection connection = {};
            connection.fd = client;
            connection.client_ip = inet_ntoa(client_addr.sin_addr);
            connection.client_port = ntohs(client_addr.sin_port);
            connection.acceptedTime = std::chrono::steady_clock::now();
            std::cout << "[Accept Loop Selected] Accepted connection from "
            << connection.client_ip   // converts s_addr to "x.x.x.x"
            << ":" << connection.client_port << "\n";
            // std::cout << connection.keepAlive << " keepalive test accept\n" << std::flush;
            loops[count % serverConfig.n_select_loops].addConnection(connection); 
            count++;
        }
        else {
            if (!server_running) {
                break;
            }
            perror("accept error");
            continue;
        }
    }    
}