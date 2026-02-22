#pragma once
#include <string>
#include <vector>
#include <unordered_map>

struct VirtualHost {
    std::string serverName;
    std::string documentRoot;
    int port = 0;
};

struct ServerConfig {
    int listen_port = -1;
    int n_threads = 0;
    int n_select_loops = 0;
    std::vector<VirtualHost> vhosts;
    std::unordered_map<std::string, VirtualHost> map; 
};

struct HtAccess {
    std::string user;
    std::string password;
    std::string authName;
};

std::string trim(const std::string &s);
ServerConfig parseConfig(std::string& path);
bool parseHtAccess(const std::string& dir, HtAccess& out);