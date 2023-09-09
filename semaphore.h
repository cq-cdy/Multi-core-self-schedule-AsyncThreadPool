//
// Created by 10447 on 2023-09-09.
//

#ifndef THREAD_POOL_SEMAPHORE_H
#define THREAD_POOL_SEMAPHORE_H
#include "mutex"
#include "condition_variable"
class Seamphore {
public:
    explicit Seamphore(int limit = 0):resLimit_(limit){}
    ~Seamphore()= default;

    void wait(){
        std::unique_lock<std::mutex> lock(mtx_);
        cond_.wait(lock,[this]() {return resLimit_ > 0;});
        resLimit_ -- ;
    }

    void post(){
        std::unique_lock<std::mutex> lock(mtx_);
        resLimit_ ++ ;
        cond_.notify_all();
    }
private:
    int resLimit_;
    std::mutex mtx_;
std::condition_variable cond_;

};

#endif //THREAD_POOL_SEMAPHORE_H
