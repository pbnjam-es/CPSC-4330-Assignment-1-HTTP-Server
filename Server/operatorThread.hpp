#pragma once
#include <atomic>
#include "connectionQueue.hpp"

class ConnectionQueue; 

extern std::atomic<bool> server_running;
void operatorThread(int listening_fd, ConnectionQueue &queue);