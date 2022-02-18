#ifndef INACTIVE_HANDLER_H
#define INACTIVE_HANDLER_H

#include <errno.h>
#include <sys/epoll.h>
#include <stdio.h>
#include "tools_func.h"
#include "lst_timer.h"

class SortTimerLst;

class InactiveHandler
{
private:
    InactiveHandler();
    ~InactiveHandler();
public:
    static InactiveHandler* instance();
    static void sigHandler(int sig);
    void timerHandler();

public:
    static int sig_pipefd_[2];

    static int epollfd_;

    SortTimerLst* timers_;

    const int time_slot_ = 5;
};

class HttpConnection;

// 定时器回调函数
void cbFunc(HttpConnection* user_data);

#endif // INACTIVE_HANDLER_H
