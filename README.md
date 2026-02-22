# CPSC-4330-Assignment-1-HTTP-Server

All code is in the directory *Server*. 

config.hpp/cpp stores helper functions for parsing external configuration files, such as .htaccess and httpd.conf

connection.hpp/connectionQueue.hpp define the channels/connections formed by accepted clients and the server. 

helpers.hpp/cpp stores helper functions for the worker threads / select loops in processing the http request

main.cpp initializes operator/worker threads, and calls parsing functions / listening socket initialization / connection acceptance functions from other files to run the server. 

operatorThread.hpp/cpp stores the thread with functionality to shutdown the server.

selectLoop.hpp/cpp implement select() pooling/multiplexing functionality for worker threads. 

server.cpp / include/server.hpp implement connection acceptance functions under both threaded and select server modes. 

workerThread.hpp/cpp stores functions for processing / responding to / executing http request, for both threaded and select server modes, as well as the main function used in thread pooling. 

Directory *contents* stores data that may be accessed through the server, at the first virtual host www.a.com 

Directory *bontents* stores data that may be accessed at the second virtual host, www.b.com. 