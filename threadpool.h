//
// Created by 10447 on 2023-09-05.
//

#ifndef THREAD_POOL_THREADPOOL_H
#define THREAD_POOL_THREADPOOL_H

#include <vector>
#include "queue"
#include "memory"
#include "atomic"
#include "thread"
#include "mutex"
#include "condition_variable"
#include "functional"
#include "any.h"
#include "semaphore.h"

using uint = unsigned int;
class Result;
class Task {
public:

    Task();
    virtual void exec() final;

    virtual Any run() = 0;

private:
    friend Result;
    void setResult(Result * res);
    Result * result_{};
};

enum class PoolMode {
    MODE_FIXED,
    MODE_CACHED,
};

class Thread {
public:
    using ThreadFunc = std::function<void()>;

    void launch();

    explicit Thread(ThreadFunc func);

    ~Thread();

private:
    ThreadFunc func_;
};

class Result {
public:
    Result(std::shared_ptr<Task> task,bool isValid = true);
    Any get();
    void setVal(Any any);

private:
    Seamphore sem_;
    Any any_;
    std::shared_ptr<Task> task_;
    bool isValid_ ;
};

class Threadpool {
public:

    Threadpool();

    ~Threadpool();

    Threadpool(const Threadpool &) = delete;

    Threadpool &operator=(const Thread &) = delete;

    void start(uint);

    void setMode(PoolMode);

    void setTaskQuemaskThreshHold(uint threshhold);

    Result submitTask(const std::shared_ptr<Task>& sp);

private:
    [[noreturn]] void threadFunc();

private:
    std::vector<std::unique_ptr<Thread>> threads_;
    uint initThreadSize_{};
    std::queue<std::shared_ptr<Task>> taskQue_;
    std::atomic_uint taskSize_{};
    uint taskQuemaskThreshHold_ = 100;
    std::mutex taskQueMtx_;
    std::condition_variable notFull_;
    std::condition_variable notEmpty_;
    PoolMode poolMode_;

};


#endif //THREAD_POOL_THREADPOOL_H
