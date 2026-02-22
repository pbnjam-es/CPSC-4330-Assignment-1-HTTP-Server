#include "config.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <iostream>

 std::string trim(const std::string &s) {
    auto start = s.find_first_not_of(" \t\r\n");
    auto end = s.find_last_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    return s.substr(start, end - start + 1);
}

ServerConfig parseConfig(std::string& path) {
    ServerConfig cfg;
    std::ifstream in(path);
    if (!in) throw std::runtime_error("Cannot open config");

    std::string line;
    VirtualHost current;
    bool in_vhost = false;

    while (std::getline(in, line)) {
        line = trim(line);
        if (line.starts_with("<VirtualHost")) {
            if (in_vhost) throw std::runtime_error("Nested VirtualHost");
            in_vhost = true;
            current = VirtualHost{};

            auto colon = line.find(':');
            auto end_bracket = line.find('>');
            if (colon == std::string::npos || end_bracket == std::string::npos)
                throw std::runtime_error("Invalid <VirtualHost> format");
            current.port = std::stoi(line.substr(colon+1, end_bracket - colon - 1));
            continue;
        }

        if (line == "</VirtualHost>") {
            if (!in_vhost) throw std::runtime_error("Unmatched </VirtualHost>");
            if (current.serverName.empty() || current.documentRoot.empty())
                throw std::runtime_error("Incomplete VirtualHost");
            cfg.vhosts.push_back(current);
            cfg.map[current.serverName] = current;
            in_vhost = false;
            continue;
        }

        std::istringstream iss(line);
        std::string key, value;
        iss >> key >> value;
        value = trim(value);

        if (in_vhost) {
            if (key == "ServerName") current.serverName = value;
            else if (key == "DocumentRoot") current.documentRoot = value;
        }
        else {
            if (key == "Listen") cfg.listen_port = std::stoi(value);
            else if (key == "nThreads") cfg.n_threads = std::stoi(value);
            else if (key == "nSelectLoops") cfg.n_select_loops = std::stoi(value);
        }
    }
    if (cfg.listen_port <= 0) throw std::runtime_error("Listen missing");
    if (cfg.vhosts.empty()) throw std::runtime_error("No VirtualHost");

    if (cfg.n_threads && cfg.n_select_loops)
        throw std::runtime_error("Choose threads OR select");

    return cfg;
}

bool parseHtAccess(const std::string& dir, HtAccess& out) {
    std::ifstream file(dir + "/.htaccess");
    if (!file) return false;

    std::string line;
    while (std::getline(file, line)) {
        if (line.starts_with("User "))
            out.user = line.substr(5);
        else if (line.starts_with("Password "))
            out.password = line.substr(9);
        else if (line.starts_with("AuthName "))
            out.authName = line.substr(9);
    }
    return true;
}