#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <sys/epoll.h>

#include "tools_func.h"
#include "locker.h"
#include "threadpool.h"
#include "http_connection.h"
#include "inactive_handler.h"

const int MAX_FD = 65536;           //最大文件描述符
const int MAX_EVENT_NUMBER = 10000; //最大事件数
const int TIMESLOT = 5;             //最小超时单位

// template <typename T>
// class threadpool;

class WebServer
{
public:
    WebServer();
    ~WebServer();

    void initThreadpool();
    void eventListen();
    void eventLoop();
    void initConn(int connfd, sockaddr_in client_address);
    void connHandler();
    void hupHandler(int connfd);
    bool sigHandler(bool& timeout, bool& stop_server);
    void readHandler(int sockfd);
    void writeHandler(int sockfd);
    void showError(int connfd, const char* info);

public:
    int port_ = 9002;
    const char* root_ = "/root";
    int epollfd_;
    HttpConnection* users_;
    
    threadpool<HttpConnection>* pool_;
    int thread_num_;
    epoll_event events_[MAX_EVENT_NUMBER];

    int listenfd_;
};


#endif // WEBSERVER_H
