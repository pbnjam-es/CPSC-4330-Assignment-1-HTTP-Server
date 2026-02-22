#include <sys/select.h>
#include <unistd.h>
#include <mutex>
#include <chrono>
#include <thread>
// #include <algorithm>

#include "config.hpp"
#include "connection.hpp"
#include "selectLoop.hpp"
#include "workerThread.hpp"
#include "helpers.hpp"



void SelectLoop::addConnection(Connection &conn) {
    std::lock_guard<std::mutex> lock(this->mutex_);
    this->connections.push_back(conn);
}

void SelectLoop::run() {
    while (server_running) {
        fd_set readset;
        FD_ZERO(&readset);
        int maxfd = 0;
        {
            std::lock_guard<std::mutex> lock(this->mutex_);
            for (auto& conn : this->connections) {
                FD_SET(conn.fd, &readset);
                if (conn.fd > maxfd) maxfd = conn.fd;
            }
        }
        if (maxfd == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        struct timeval tv = {3, 0};
        int ready = select(maxfd+1, &readset, nullptr, nullptr, &tv);
        if (ready <= 0) {
            continue;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto it = connections.begin(); it != connections.end();) {
            if (FD_ISSET(it->fd, &readset)) {
                it->readBuffer.clear();
                std::cout << "[Worker] Processing connection from "
                << it->client_ip << ":" << it->client_port << "\n";
                

                if (!readWithNonBlocking(*it, it->readBuffer, 3)) {
                    std::cout << "Connection timed out " << it->client_ip << "\n";
                    close(it->fd);
                    it = connections.erase(it);
                    continue;
                }
                parseHttpRequest(*it);
                if (it->request.url == "/load") {
                    handleLoadRequestSelected(*it, connections, 50);
                    close(it->fd);
                    it = connections.erase(it);
                    continue;
                }
                VirtualHost* vh = resolveVhost(this->vhosts, it->request);
                // sendBasicResponse(*it);
                std::string path = buildPath(*vh, it->request);

                if (!checkAuth(*it, path)) {
                    close(it->fd);
                    std::cout << "Incorrect Authentication" << "\n";
                    it = connections.erase(it);
                    continue;
                }
                if (isExecutable(path)) {
                    executeCgi(*it, path, *vh);
                }
                else {
                    servePath(*it, path);
                }
                
                if (!it->keepAlive) {
                    close(it->fd);
                    it = connections.erase(it);
                    continue;
                }

                it->readBuffer.clear();
                it->request = {};
            }
        }
    }
}