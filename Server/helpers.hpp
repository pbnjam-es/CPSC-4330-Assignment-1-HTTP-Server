#include <ctime>
#include "connection.hpp"
#include "connectionQueue.hpp"
#include "config.hpp"

std::string httpDate(std::time_t t);
VirtualHost* resolveVhost(std::vector<VirtualHost>& vhosts, HttpRequest& httpRequest);
std::string resolveUserAgent(HttpRequest& httpRequest);
std::string sanitizeUrl(HttpRequest& httpRequest);
std::string fileType(std::string path);
bool isExecutable(std::string& path);
std::vector<std::string> build_envp(Connection& conn, VirtualHost& vhost);
std::string base64Decode(const std::string& in);
bool acceptContentType(Connection &conn, std::string &contentType);