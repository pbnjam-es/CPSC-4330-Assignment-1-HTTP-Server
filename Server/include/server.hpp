#include <netinet/in.h>
#include <sys/socket.h>
#include "../selectLoop.hpp"

int startListeningSocket(int port);
void setNonBlocking(int fd);
void runAcceptLoopThreaded(int listen_fd, ServerConfig &serverConfig, ConnectionQueue &connectionQueue);
void runAcceptLoopSelected(int listen_fd, ServerConfig &serverConfig, std::vector<SelectLoop> &loops);
