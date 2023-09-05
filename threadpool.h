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
class Task {
public:
    virtual void run() = 0;
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

class Threadpool {
public:

    Threadpool();

    ~Threadpool();

    Threadpool(const Threadpool &) = delete;

    Threadpool &operator=(const Thread &) = delete;

    void start(unsigned int);

    void setMode(PoolMode);

    void setTaskQuemaskThreshHold(unsigned int threshhold);

    void submitTask(const std::shared_ptr<Task> &sp);

private:
    static void threadFunc();

private:
    using uint = unsigned int;
    std::vector<Thread *> threads_;
    uint initThreadSize_{};
    std::queue<std::shared_ptr<Task>> taskQue_;
    std::atomic_uint taskSize_{};
    uint taskQuemaskThreshHold_ = 100;
    std::mutex taskQueMtx;
    std::condition_variable notFull_;
    std::condition_variable notEmpty_;
    PoolMode poolMode_;

};


#endif //THREAD_POOL_THREADPOOL_H
