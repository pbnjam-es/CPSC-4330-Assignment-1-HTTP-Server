#include <atomic>
#include <iostream>
#include <unistd.h>
#include <thread>
#include "operatorThread.hpp"
// 
std::atomic<bool> server_running(true);

void operatorThread(int listening_fd, ConnectionQueue &queue) {
    // server_running.store(true);
    std::string cmd;
    while (server_running) {
        if (!getline(std::cin, cmd)) {
            // stdin closed/EOF, just wait
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
        }
        if (cmd == "shutdown") {
            std::cout << "[Management] Shutting down server...\n";
            server_running = false;
            close(listening_fd);
            queue.shutdown();
        }
    }
}