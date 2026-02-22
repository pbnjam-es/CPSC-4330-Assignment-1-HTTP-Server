#pragma once 
#include <queue>
#include <mutex>
#include <condition_variable>
#include <iostream>

#include "operatorThread.hpp"
#include "connection.hpp"


class ConnectionQueue {
private:
    bool closed_ = false;
    std::mutex mutex_;
    std::condition_variable cv_;
public:
    std::queue<Connection> connection_queue;
    void push(Connection connection) {
        std::unique_lock<std::mutex>  lock(mutex_);
        if (closed_) {
            return;
        }
        // std::cerr << "PUSH fd=" << connection.fd << " ip=" << connection.client_ip << "\n";
        connection_queue.push(connection);
        cv_.notify_one();
    }

    bool pop(Connection &connection) {
        std::unique_lock<std::mutex>  lock(mutex_);
        
        if (!cv_.wait_for(lock, std::chrono::seconds(3), [&] {
            return closed_ || !connection_queue.empty();
        }))
        {

        }

        if (connection_queue.empty()) {
            // closed AND no work left
            return false;
        }
        // cv_.wait(lock, [&]{ return !connection_queue.empty();});
        connection = std::move(connection_queue.front());
        connection_queue.pop();
        // std::cerr << "POP fd=" << connection.fd << " ip=" << connection.client_ip << "\n";
        // task.request.headers["host"] = "localhost";
        return true;
    }
    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        closed_ = true;
        cv_.notify_all();  // 🔥 wake all workers
    }

    size_t size() {
        std::lock_guard<std::mutex> lock(mutex_);
        return connection_queue.size();
    }
};