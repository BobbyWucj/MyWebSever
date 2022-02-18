#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "locker.h"

// 参数 T 是任务类，主线程将任务插入请求队列，子线程通过竞争获得任务
template <typename T>
class threadpool
{
public:
    threadpool(int thread_number = 8, int max_requests = 10000);
    ~threadpool();

    bool append(T* request);
private:
    static void* worker(void* arg);
    void run();

private:
    // 线程池中的线程数
    int thread_number_;
    // 请求队列中允许的最大请求数
    int max_requests_;
    // 描述线程池的数组
    pthread_t* threads_;
    // 请求队列
    std::list<T*> work_queue_;
    // 请求队列的互斥锁
    Locker queue_locker_;
    // 是否有任务需要处理
    Sem queue_stat_;
    // 是否结束线程
    bool stop_;
};

template <typename T>
threadpool<T>::threadpool(int thread_number, int max_requests)
    : thread_number_(thread_number),
      max_requests_(max_requests),
      threads_(NULL),
      stop_(false)
{
    if(thread_number <= 0 || max_requests <= 0) {
        throw std::exception();
    }
    threads_ = new pthread_t[thread_number_];
    if(!threads_) {
        throw std::exception();
    }

    for(int i = 0; i < thread_number_; ++i) {
        printf("create the %dth thread\n", i);
        if(pthread_create(threads_ + i, NULL, worker, this) != 0) {
            delete [] threads_;
            throw std::exception();
        }
        if(pthread_detach(threads_[i])) {
            delete [] threads_;
            throw std::exception();
        }
    }
}

template <typename T>
inline threadpool<T>::~threadpool() {
    delete [] threads_;
    stop_ = true;
}

template <typename T>
inline bool threadpool<T>::append(T* request) {
    // 操作请求队列时需要加锁，因为其被所有线程共享
    queue_locker_.lock();
    if(static_cast<int>(work_queue_.size()) > max_requests_) {
        queue_locker_.unlock();
        return false;
    }
    work_queue_.push_back(request);
    queue_locker_.unlock();
    queue_stat_.post();
    return true;
}

/**
 * @description: 工作线程.由于在pthread_create中的函数只能是静态函数，
 *                  所以只能通过参数传入or静态对象的方法访问类内动态成员
 * @param {arg} 传入的this指针
 * @return {void*} 返回线程池指针
 */
template <typename T>
inline void* threadpool<T>::worker(void* arg) {
    threadpool* pool = (threadpool*)arg;
    pool->run();
    return pool;
}

// 工作线程的实际运行函数
template <typename T>
void threadpool<T>::run() {
    while(!stop_) {
        queue_stat_.wait();
        queue_locker_.lock();
        if(work_queue_.empty()) {
            queue_locker_.unlock();
            continue;
        }
        T* request = work_queue_.front();
        work_queue_.pop_front();

        queue_locker_.unlock();
        if(!request) {
            continue;
        }
        request->process();
    }
}

#endif // THREADPOOL_H
