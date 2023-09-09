#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <iostream>
#include <utility>
#include <vector>
#include <queue>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <unordered_map>
#include <thread>
#include <future>

const int TASK_MAX_THRESHHOLD = 1024;
const int THREAD_MAX_THRESHHOLD = 1024;
const int THREAD_MAX_IDLE_TIME_S = 60;


enum class PoolMode
{
    MODE_FIXED,  // 固定数量的线程
    MODE_CACHED, // 线程数量可动态增长
};

class Thread
{
public:
    using ThreadFunc = std::function<void(int)>;

    explicit Thread(ThreadFunc func)
            : func_(std::move(func))
            , threadId_(generateId_++)
    {}
    ~Thread() = default;

    void start()
    {
        std::thread t(func_, threadId_);
        t.detach();
    }

    // 获取线程id
    [[nodiscard]] int getId()const
    {
        return threadId_;
    }
private:
    ThreadFunc func_;
    static int generateId_;
    int threadId_;
};

int Thread::generateId_ = 0;

class ThreadPool
{
public:
    ThreadPool()
            : initThreadSize_(0)
            , taskSize_(0)
            , idleThreadSize_(0)
            , curThreadSize_(0)
            , taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD)
            , threadSizeThreshHold_(THREAD_MAX_THRESHHOLD)
            , poolMode_(PoolMode::MODE_FIXED)
            , isPoolRunning_(false)
    {}

    ~ThreadPool()
    {
        isPoolRunning_ = false;

        std::unique_lock<std::mutex> lock(taskQueMtx_);
        notEmpty_.notify_all();
        exitCond_.wait(lock, [&]()->bool {return threads_.empty(); });
    }

    void setMode(PoolMode mode)
    {
        if (checkRunningState()){
            std::cout << " already start!\n";
            return;
        }
        poolMode_ = mode;
    }

    void setTaskQueMaxThreshHold(int threshhold)
    {
        if (checkRunningState())
            return;
        taskQueMaxThreshHold_ = threshhold;
    }

    void setThreadSizeThreshHold(int threshhold)
    {
        if (checkRunningState())
            return;
        if (poolMode_ == PoolMode::MODE_CACHED)
        {
            threadSizeThreshHold_ = threshhold;
        }
    }


    template<typename Func, typename... Args>

    /**
     * packaged_task用来包函数
     * 然后 packaged_task类里面有一个 类似于Any类 的 手法，
     * 将函数返回值存放到 Any 成员
     * ，并且，将packaged_task中的 future对象 返回给用户
     * 当用户 调用 future的get方法时，如果内部实现的信号量
     * 还没有，就会阻塞等待，等 函数执行完 ，把函数 的返回值
     * 给到Any成员时，就会唤醒阻塞的get，并将返回值给从get返回给用户
     */
    auto submitTask(Func&& func, Args&&... args) -> std::future<decltype(func(args...))>
    {
        using RType = decltype(func(args...));
        auto task = std::make_shared<std::packaged_task<RType()>>(
                std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
        std::future<RType> result = task->get_future();
        std::unique_lock<std::mutex> lock(taskQueMtx_);
        if (!notFull_.wait_for(lock, std::chrono::seconds(1),
                               [&]()->bool { return taskQue_.size() < (size_t)taskQueMaxThreshHold_; }))
        {
            std::cerr << "task queue is full, submit task fail." << std::endl;
            auto task_ = std::make_shared<std::packaged_task<RType()>>(
                    []()->RType { return RType(); });
            (*task_)();
            return task_->get_future();
        }

        taskQue_.emplace([task]() {(*task)();});
        taskSize_++;

        notEmpty_.notify_all();

        if (poolMode_ == PoolMode::MODE_CACHED
            && taskSize_ > idleThreadSize_
            && curThreadSize_ < threadSizeThreshHold_)
        {
            std::cout << ">>> create new thread..." << std::endl;
            auto ptr = std::make_unique<Thread>([this](auto && PH1) { threadFunc(std::forward<decltype(PH1)>(PH1)); });
            int threadId = ptr->getId();
            threads_.emplace(threadId, std::move(ptr));
            threads_[threadId]->start();
            curThreadSize_++;
            idleThreadSize_++;
        }

        return result;
    }

    // 开启线程池
    void start(int initThreadSize =(int)std::thread::hardware_concurrency())
    {
        isPoolRunning_ = true;

        initThreadSize_ = initThreadSize;
        curThreadSize_ = initThreadSize;

        for (int i = 0; i < initThreadSize_; i++)
        {

            auto ptr = std::make_unique<Thread>([this](auto && PH1) { threadFunc(std::forward<decltype(PH1)>(PH1)); });
            int threadId = ptr->getId();
            threads_.emplace(threadId, std::move(ptr));

        }

        for (int i = 0; i < initThreadSize_; i++)
        {
            threads_[i]->start();
            idleThreadSize_++;
        }
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

private:
    /*
     * 定义线程函数, 此线程池没有管理线程
     * 每个线程函数 执行的时候就会判断是否需要线程数缩减
     * 而是否能增加线程数，是在submitTask哪里做的。
     */
    void threadFunc(int threadid)
    {
        auto lastTime = std::chrono::high_resolution_clock::now();

        for (;;)
        {
            Task task;
            {
                std::unique_lock<std::mutex> lock(taskQueMtx_);

                std::cout << "tid:" << std::this_thread::get_id()
                          << "try to get task ..." << std::endl;




                // 锁 + 双重判断
                while (taskQue_.empty())
                {
                    // 线程池要结束，回收线程资源
                    if (!isPoolRunning_)
                    {
                        threads_.erase(threadid); // std::this_thread::getid()
                        std::cout << "threadid:" << std::this_thread::get_id() << " exit!"
                                  << std::endl;
                        exitCond_.notify_all();
                        return;
                    }

                    if (poolMode_ == PoolMode::MODE_CACHED)
                    {
                        if (std::cv_status::timeout ==
                            notEmpty_.wait_for(lock, std::chrono::seconds(1)))
                        {
                            auto now = std::chrono::high_resolution_clock::now();
                            auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
                            if (dur.count() >= THREAD_MAX_IDLE_TIME_S
                                && curThreadSize_ > initThreadSize_)
                            {

                                threads_.erase(threadid);
                                curThreadSize_--;
                                idleThreadSize_--;

                                std::cout << "threadid:" << std::this_thread::get_id() << " exit!"
                                          << std::endl;
                                return;
                            }
                        }
                    }
                    else
                    {
                        notEmpty_.wait(lock);
                    }
                }

                idleThreadSize_--;

                std::cout << "tid:" << std::this_thread::get_id()
                          << "get task..." << std::endl;

                task = taskQue_.front();
                taskQue_.pop();
                taskSize_--;

                if (!taskQue_.empty())
                {
                    notEmpty_.notify_all();
                }

                notFull_.notify_all();
            }
            if (task != nullptr)
            {
                task();
            }
            idleThreadSize_++;
            lastTime = std::chrono::high_resolution_clock::now();
        }
    }
    bool checkRunningState() const
    {
        return isPoolRunning_;
    }

private:
    std::unordered_map<int, std::unique_ptr<Thread>> threads_;

    int initThreadSize_;
    int threadSizeThreshHold_;
    std::atomic_int curThreadSize_;
    std::atomic_int idleThreadSize_;

    using Task = std::function<void()>;
    std::queue<Task> taskQue_;
    std::atomic_int taskSize_;
    int taskQueMaxThreshHold_;

    std::mutex taskQueMtx_;
    std::condition_variable notFull_;
    std::condition_variable notEmpty_;
    std::condition_variable exitCond_;

    PoolMode poolMode_;
    std::atomic_bool isPoolRunning_;
};

#endif
