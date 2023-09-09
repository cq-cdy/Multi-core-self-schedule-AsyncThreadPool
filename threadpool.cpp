//
// Created by 10447 on 2023-09-05.
//
#include "threadpool.h"
#include <utility>
#include "iostream"

const uint TASK_MAX_THRESHHOLD = 1024;
const uint HARDWARE_CONCURRENCY_NUM = std::thread::hardware_concurrency();

Threadpool::Threadpool()
        : initThreadSize_(0), taskSize_(0), taskQuemaskThreshHold_(TASK_MAX_THRESHHOLD),
          poolMode_(PoolMode::MODE_FIXED) {}

Threadpool::~Threadpool() = default;

void Threadpool::start(uint size = HARDWARE_CONCURRENCY_NUM) {
    initThreadSize_ = size;
    // 创建线程对象
    for (int i = 0; i < initThreadSize_; ++i) {
        threads_.emplace_back(new Thread([this] { threadFunc(); }));
    }
    //启动线程
    for (int i = 0; i < initThreadSize_; ++i)
        threads_[i]->launch();
}

class Task;

Result Threadpool::submitTask( const std::shared_ptr<Task>& sp) {
    std::unique_lock<std::mutex> lock(taskQueMtx_);
    if (!notFull_.wait_for(
            lock,
            std::chrono::milliseconds(1000),
            [&]() -> bool {
                return taskQue_.size() < taskQuemaskThreshHold_;
            })) {
        std::cerr << " task submit time out" << std::endl;
        return {sp, false};
    }
    taskQue_.emplace(sp);
    taskSize_++;
    notEmpty_.notify_all(); // 叫线程来拿任务
    return {sp,true};
}

void Threadpool::setMode(PoolMode mode) {
    poolMode_ = mode;
}

void Threadpool::setTaskQuemaskThreshHold(uint threshhold) {
    taskQuemaskThreshHold_ = threshhold;
}

[[noreturn]] void Threadpool::threadFunc() { // 从队列取任务
    //std::cout << "begin:" << std::this_thread::get_id() << std::endl;
    for (;;) {
        std::shared_ptr<Task> task;
        {
            std::unique_lock<std::mutex> lock(taskQueMtx_);
            notEmpty_.wait(lock, [this]() -> bool {
                return !taskQue_.empty();
            });
            task = taskQue_.front();
            taskQue_.pop();
            taskSize_--;
            if (!taskQue_.empty()) { notEmpty_.notify_all(); }
            notFull_.notify_all(); // 如果不通知 ，那么等待的线程可能超时而挂掉
        } // relase lock
        if (task != nullptr) {
            task->exec();
        }
    }
}

Thread::Thread(Thread::ThreadFunc func) : func_(std::move(func)) {}

Thread::~Thread() = default;

void Thread::launch() {
    std::thread t(func_);
    t.detach();
}

Result::Result(std::shared_ptr<Task> task, bool isValid) : isValid_(isValid), task_(std::move(task)) {
    task_->setResult(this);
}

Any Result::get() {
    if (!isValid_) {
        return "";
    }
    sem_.wait();
    return std::move(any_);
}

void Result::setVal(Any any) {
    this->any_ = std::move(any);
    sem_.post(); // 已经拿到返回值，增加信号量资源
}

void Task::exec() {
    if (result_ != nullptr) {
        result_->setVal(std::move(run()));
    }
}

void Task::setResult(Result *res) {
    result_ = res;
}

Task::Task() : result_(nullptr) {

}
