#pragma once

class SelectLoop {
private:
    std::vector<Connection> connections;
    std::mutex mutex_;
    std::vector<VirtualHost> vhosts;
public:
    SelectLoop(std::vector<VirtualHost>& vhosts) : vhosts(vhosts) {}
    SelectLoop(SelectLoop&& other) 
    : connections(std::move(other.connections)),
      vhosts(other.vhosts) {}
      
    void addConnection(Connection &conn);
    void run();
};