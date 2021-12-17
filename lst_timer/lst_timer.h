#ifndef LST_TIMER_H
#define LST_TIMER_H

#include <time.h>
#include "netinet/in.h"
#include "../http_connection/http_connection.h"
#define BUFFER_SIZE 64

class UtilTimer;

class HttpConnection;

class UtilTimer
{
public:
    UtilTimer(): prev_(nullptr), next_(nullptr), user_data_(nullptr), cb_func_(nullptr) {}
    UtilTimer(time_t expire, void (*cb_func)(HttpConnection*), HttpConnection* user_data);
public:
    time_t expire_;
    void (*cb_func_)(HttpConnection *);
    HttpConnection* user_data_;
    UtilTimer* prev_;
    UtilTimer* next_;
};

class SortTimerLst
{
private:
    UtilTimer* head_;
    UtilTimer* tail_;

    void addTimer(UtilTimer *timer, UtilTimer *lst_head);
public:
    SortTimerLst();
    ~SortTimerLst();

    void addTimer(UtilTimer *timer);
    void adjustTimer(UtilTimer *timer);
    void delTimer(UtilTimer *timer);
    void tick();
};

#endif // LST_TIMER_H
