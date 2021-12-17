#include "web_server.h"

WebServer::WebServer() {
    users_ = new HttpConnection[MAX_FD];
}

WebServer::~WebServer()
{
    close(epollfd_);
    close(listenfd_);
    delete [] users_;
    delete pool_;
}

void WebServer::initThreadpool() {
    pool_ = new threadpool<HttpConnection>();
}

void WebServer::eventListen() {
    listenfd_ = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd_ >= 0);

    struct linger sock_option = {0, 1};
    setsockopt(listenfd_, SOL_SOCKET, SO_LINGER, &sock_option, sizeof(sock_option));

    int ret = 0;
    sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    // inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port_);

    int flag = 1;
    setsockopt(listenfd_, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));

    ret = bind(listenfd_, (sockaddr*)&address, sizeof(address));
    assert(ret != -1);

    ret = listen(listenfd_, 5);
    assert(ret != -1);

    epollfd_ = epoll_create(5);
    assert(epollfd_ != 1);
#ifdef LISTEN_ET
    addFd(epollfd_, listenfd_, false, true);
    printf("Listenfd is on ET mode\n");
#else
    addFd(epollfd_, listenfd_, false, false);
    printf("Listenfd is on LT mode\n");
#endif

    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, InactiveHandler::sig_pipefd_);
    assert(ret != -1);

    addFd(epollfd_, InactiveHandler::sig_pipefd_[0], false, false);
    setNonBlocking(InactiveHandler::sig_pipefd_[1]);
    HttpConnection::epollfd_ = epollfd_;
    InactiveHandler::epollfd_ = epollfd_;

    addSig(SIGPIPE, SIG_IGN);
    addSig(SIGALRM, InactiveHandler::sigHandler, false);
    addSig(SIGTERM, InactiveHandler::sigHandler, false);
    
    alarm(TIMESLOT);
}

void WebServer::eventLoop() {
    bool timeout = false;
    bool stop_server = false;

    while(!stop_server) {
        int number = epoll_wait(epollfd_, events_, MAX_EVENT_NUMBER, -1);
        if(number < 0 && errno != EINTR) {
            printf("epoll fail\n");
            break;
        }

        for(int i = 0; i < number; ++i) {
            int sockfd = events_[i].data.fd;

            if(sockfd == listenfd_) {
                connHandler();
            } else if(events_[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                hupHandler(sockfd);
            } else if(sockfd == InactiveHandler::sig_pipefd_[0] && (events_[i].events & EPOLLIN)) {
                bool flag = sigHandler(timeout, stop_server);
            } else if(events_[i].events & EPOLLIN) {
                readHandler(sockfd);
            } else if(events_[i].events & EPOLLOUT) {
                writeHandler(sockfd);
            }
        }
        if(timeout) {
            HttpConnection::inactive_handler_->timerHandler();
            timeout = false;
        }
    }
}

void WebServer::connHandler() {
    struct sockaddr_in client_address;
    socklen_t client_addrlen = sizeof(client_address);
#ifdef LISTEN_ET
    while (1)
    {
        int connfd = accept(listenfd_, (struct sockaddr*)&client_address, &client_addrlen);
        if(connfd < 0) {
            if(errno == EWOULDBLOCK || errno == EAGAIN) {
                break;
            }
            printf("errno is : %d\n", errno);
            break;
        }
        if(HttpConnection::user_count_ >= MAX_FD) {
            showError(connfd, "Internal sever busy\n");
            break;
        }
        users_[connfd].init(connfd, client_address);
    }
#else
    int connfd = accept(listenfd_, (struct sockaddr*)&client_address, &client_addrlen);
    if(connfd < 0) {
        printf("errno is : %d\n", errno);
        return;
    }
    if(HttpConnection::user_count_ >= MAX_FD) {
        showError(connfd, "Internal sever busy\n");
        return;
    }
    users_[connfd].init(connfd, client_address);
#endif
}

void WebServer::hupHandler(int connfd) {
    users_[connfd].closeConn();
}

bool WebServer::sigHandler(bool& timeout, bool& stop_server) {
    int sig;
    int ret = 0;
    int* sig_pipe = InactiveHandler::sig_pipefd_;
    char signals[1024];
    ret = recv(sig_pipe[0], signals, sizeof(signals), 0);
    if(ret == -1) {
        printf("sig pipe error\n");
        return false;
    } else if(ret == 0) {
        return false;
    } else {
        for(int k = 0; k < ret; ++k) {
            switch (signals[k])
            {
            case SIGALRM:
                timeout = true;
                break;
            case SIGTERM:
                stop_server = true;
                break;
            }
        }
    }
    return true;
}

void WebServer::readHandler(int sockfd) {
    if(users_[sockfd].read()) {
        pool_->append(users_ + sockfd);
        // 刷新定时器超时时间
        users_[sockfd].adjustTimer();
    } else {
        users_[sockfd].closeConn();
    }
}

void WebServer::writeHandler(int sockfd) {
    if(!users_[sockfd].write()) {
        users_[sockfd].closeConn();
    }
}

void WebServer::showError(int connfd, const char* info) {
    printf("%s", info);
    send(connfd, info, strlen(info), 0);
    close(connfd);
}
