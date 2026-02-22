#pragma once
#include <string>
#include <sys/socket.h>
#include <chrono> 

enum CONNECTIONSTATE {
    ACCEPTED, 
    READING,
    WRITING, 
    CLOSED
};


struct HttpRequest {
    std::string method;
    std::string url;
    std::string version;
    std::unordered_map<std::string, std::string> headers;
    std::string queryString;
    std::string body;
};

struct Connection {
    int fd = -1;
    CONNECTIONSTATE connectionState = CONNECTIONSTATE::ACCEPTED;
    bool keepAlive = true;
    std::chrono::steady_clock::time_point acceptedTime;
    std::string client_ip;
    int client_port;
    std::string readBuffer;
    HttpRequest request;
};
