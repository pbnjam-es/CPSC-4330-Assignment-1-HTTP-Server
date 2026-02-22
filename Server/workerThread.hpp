#include "config.hpp"
#include "connectionQueue.hpp"

void workerThread(ConnectionQueue& connection_queue, std::vector<VirtualHost>& vhosts);
bool readWithNonBlocking(Connection &connection, std::string &buffer, int timeoutSeconds);
void handleLoadRequestThreaded(Connection &conn, ConnectionQueue& connection_queue, size_t max);
void handleLoadRequestSelected(Connection &conn, std::vector<Connection>& connection_queue, size_t max);
void parseHttpRequest(Connection &conn);
// void sendBasicResponse(Connection &conn);

std::string buildPath(VirtualHost& vhost, HttpRequest& httpRequest);
bool checkAuth(Connection &conn, std::string &path);
void send304(Connection &conn, struct stat &st);
void send401(Connection &conn, HtAccess htaccess);
void send404(Connection &conn);
void send403(Connection &conn);
std::string fileType(std::string path);
void servePath(Connection &conn, std::string path);
void executeCgi(Connection& conn, std::string& path, VirtualHost& vhost);