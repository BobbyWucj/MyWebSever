#include "inactive_handler.h"

int InactiveHandler::sig_pipefd_[2];
int InactiveHandler::epollfd_ = -1;

InactiveHandler::InactiveHandler() {
    timers_ = new SortTimerLst();
}

InactiveHandler::~InactiveHandler() {
    delete timers_;
}

InactiveHandler* InactiveHandler::instance() {
    static InactiveHandler inactive_handler;
    return &inactive_handler;
}

void InactiveHandler::sigHandler(int sig) {
    int save_errno = errno;
    int msg = sig;
    send(sig_pipefd_[1], (char*)&msg, 1, 0);
    errno = save_errno;
}

void InactiveHandler::timerHandler() {
    timers_->tick();
    alarm(time_slot_);
}

void cbFunc(HttpConnection* user_data) {
    user_data->closeConn();
    printf("connection %d timeout! Closing now\n", user_data->sockfd_);
}