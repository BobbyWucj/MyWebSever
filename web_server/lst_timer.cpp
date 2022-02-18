#include "lst_timer.h"

SortTimerLst::SortTimerLst() : head_(NULL), tail_(NULL) {}

SortTimerLst::~SortTimerLst()
{
    UtilTimer *tmp = head_;
    while(tmp) {
        head_ = tmp->next_;
        delete tmp;
        tmp = head_;
    }
}

void SortTimerLst::addTimer(UtilTimer *timer, UtilTimer *lst_head)
{
    UtilTimer *pre = lst_head;
    UtilTimer *cur = pre->next_;
    
    while(cur) { // 找到一个cur->expire_大于timer->expire_，并插入cur前面
        if(cur->expire_ > timer->expire_) {
            pre->next_ = timer;
            timer->next_ = cur;
            timer->prev_ = pre;
            cur->prev_ = timer;
            break;
        }
        pre = cur;
        cur = cur->next_;
    }

    if(!cur) { // timer的expire_最大，插入尾部
        pre->next_ = timer;
        timer->prev_ = pre;
        timer->next_ = NULL;
        tail_ = timer;
    }
}

void SortTimerLst::addTimer(UtilTimer *timer)
{
    if(!timer) {
        return;
    }
    if(!head_) { // 当前链表为空
        head_ = tail_ = timer;
        return;
    }
    if(timer->expire_ < head_->expire_) { // 值最小，插入头部
        timer->next_ = head_;
        head_->prev_ = timer;
        head_ = timer;
        return;
    }
    addTimer(timer, head_);
}

void SortTimerLst::adjustTimer(UtilTimer *timer) // 只能处理timer值变大的情况
{
    if(!timer) {
        return;
    }
    UtilTimer *tmp = timer->next_;
    if(timer == tail_ || timer->expire_ <= tmp->expire_) { // 不需要调整
        return;
    }
    if(timer == head_) {
        head_ = head_->next_;
        head_->prev_ = NULL;
        timer->next_ = NULL;
        addTimer(timer, head_);
    } 
    else {
        timer->prev_->next_ = timer->next_;
        timer->next_->prev_ = timer->prev_;
        addTimer(timer, timer->next_);
    }  
}

void SortTimerLst::delTimer(UtilTimer *timer)
{
    if(!timer) {
        return;
    }
    if(timer == head_ && timer == tail_) { // 链表只有一个结点
        delete timer;
        head_ = NULL;
        tail_ = NULL;
        return;
    }
    if(timer == head_) { // 链表不止一个结点并且等于head_
        head_ = head_->next_;
        head_->prev_ = NULL;
        delete timer;
        return;
    }
    if(timer == tail_) { // 链表不止一个结点并且等于tail_
        tail_ = tail_->prev_;
        tail_->next_ = NULL;
        delete timer;
        return;
    }
    // timer位于中间
    timer->prev_->next_ = timer->next_;
    timer->next_->prev_ = timer->prev_;
    delete timer;
}

// SIGALRM 信号面每次触发就在其信号处理函数中执行一次 tick 函数
void SortTimerLst::tick()
{
    if(!head_) {
        return;
    }
    
    time_t cur_time = time(NULL);
    UtilTimer *tmp = head_;
    // 从head_开始遍历链表，执行所有到期的定时任务的回调函数
    while(tmp) {
        // 遍历到一个未到期的定时器则break（升序）
        if(cur_time < tmp->expire_) {
            break;
        }
        UtilTimer* node = head_->next_;
        tmp->cb_func_(tmp->user_data_);
        
        // head_ = node;
        // if(head_) {
        //     head_->prev_ = NULL;
        // }
        tmp = node;
    }
}

UtilTimer::UtilTimer(time_t expire, void (cb_func)(HttpConnection*), HttpConnection* user_data)
    : expire_(expire),
      cb_func_(cb_func),
      user_data_(user_data),
      prev_(NULL),
      next_(NULL)
{ }

