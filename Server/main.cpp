#include "config.hpp"
#include "connectionQueue.hpp"
#include "workerThread.hpp"
#include "include/server.hpp"
#include "operatorThread.hpp"
#include "selectLoop.hpp"
#include <iostream>
#include <unistd.h>
#include <string>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>


// int start_listening_socket(int);

int main(int argc, char** argv) {
    if (argc < 3 || std::string(argv[1]) != "-config") {
        std::cerr << "Correct Usage: server -config file";
    }
    std::string path = argv[2];
    ServerConfig serverConfig = parseConfig(path);
    
    ConnectionQueue connectionQueue;
    
    int listen_fd = startListeningSocket(serverConfig.listen_port);
    std::thread operator_thread(operatorThread, listen_fd, std::ref(connectionQueue));
    std::vector<std::thread> workers;
    std::cout << serverConfig.n_threads << " THREAD NUM \n" << std::flush;
    if (serverConfig.n_threads > 0) {
        for (int i = 0; i <serverConfig.n_threads; i++) {
            workers.emplace_back(workerThread, std::ref(connectionQueue), std::ref(serverConfig.vhosts));
        }
        runAcceptLoopThreaded(listen_fd, serverConfig, connectionQueue);
        for (int i = 0; i <serverConfig.n_threads; i++) {
            workers[i].join();
        }
    }
    else {
        std::cout << serverConfig.n_select_loops << " SELECT NUM \n" << std::flush;
        std::vector<SelectLoop> loops;
        loops.reserve(serverConfig.n_select_loops);
        for (int i = 0; i < serverConfig.n_select_loops; i++) {
            loops.emplace_back(serverConfig.vhosts);
        }
        for (int i = 0; i <serverConfig.n_select_loops; i++) {
            workers.emplace_back(&SelectLoop::run, &loops[i]);
        }
        runAcceptLoopSelected(listen_fd, serverConfig, loops);
        for (int i = 0; i <serverConfig.n_select_loops; i++) {
            workers[i].join();
        }
    }
    operator_thread.join();
    return 0;
}