//
// Created by 10447 on 2023-09-05.
//
#include "threadpool.h"
#include <utility>
#include "iostream"
const unsigned int TASK_MAX_THRESHHOLD = 1024;
const unsigned int HARDWARE_CONCURRENCY_NUM = std::thread::hardware_concurrency();

Threadpool::Threadpool()
        : initThreadSize_(0), taskSize_(0), taskQuemaskThreshHold_(TASK_MAX_THRESHHOLD),
          poolMode_(PoolMode::MODE_FIXED) {}

Threadpool::~Threadpool() = default;

void Threadpool::start(unsigned int size = HARDWARE_CONCURRENCY_NUM) {
    initThreadSize_ = size;
    // 创建线程对象
    for (int i = 0; i < initThreadSize_; ++i) {
        threads_.emplace_back(new Thread([this] { threadFunc(); }));
    }
    //启动线程
    for (int i = 0; i < initThreadSize_; ++i)
        threads_[i]->launch();
}

void Threadpool::submitTask(const std::shared_ptr<Task> &sp) {
}

void Threadpool::setMode(PoolMode mode) {
    poolMode_ = mode;
}

void Threadpool::setTaskQuemaskThreshHold(unsigned int threshhold) {
    taskQuemaskThreshHold_ = threshhold;
}

void Threadpool::threadFunc() {
    std::cout << "begin:" << std::this_thread::get_id() << std::endl;
}

Thread::Thread(Thread::ThreadFunc func) :func_(std::move(func)){
}

Thread::~Thread() = default;

void Thread::launch() {
    std::thread t(func_);
    t.detach();
}