#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <sched.h>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

auto lastTime = std::chrono::high_resolution_clock::now();
const int TASK_MAX_THRESHHOLD = 1024;
const int THREAD_MAX_THRESHHOLD = 1024;
const int THREAD_MAX_IDLE_TIME_S = 3;
const unsigned int NUM_CORES = std::thread::hardware_concurrency();

class Task;

class Task_Config {
public:
    Task_Config(int num_cores = -1, int priority = SCHED_OTHER, int mode = -1)
            : num_cores_(num_cores), priority_(priority), mode_(mode) {}

    friend Task;
    int num_cores_;
    int priority_;
    int mode_;
};

enum class PoolMode {
    MODE_FIXED,   // 固定数量的线程
    MODE_CACHED,  // 线程数量可动态增长
};
cpu_set_t cpuset;

class Thread {
public:
    using ThreadFunc = std::function<void(int)>;

    explicit Thread(ThreadFunc func)
            : func_(std::move(func)), threadId_(generateId_++) {}

    ~Thread() = default;

    void launch() {
        std::thread t(func_, threadId_);
        t.detach();
    }

    [[nodiscard]] int getId() const { return threadId_; }

private:
    ThreadFunc func_;
    static int generateId_;
    int threadId_;
};

int Thread::generateId_ = 0;

class ThreadPool {
public:
    ThreadPool()
            : initThreadSize_(0),
              taskSize_(0),
              idleThreadSize_(0),
              curThreadSize_(0),
              taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD),
              threadSizeThreshHold_(THREAD_MAX_THRESHHOLD),
              poolMode_(PoolMode::MODE_FIXED),
              isPoolRunning_(false) {}

    ~ThreadPool() {
        isPoolRunning_ = false;

        std::unique_lock<std::mutex> lock(taskQueMtx_);
        notEmpty_.notify_all();
        exitCond_.wait(lock, [&]() -> bool { return threads_.empty(); });

    }

    void setMode(PoolMode mode) {
        if (checkRunningState()) {
            return;
        }
        poolMode_ = mode;
    }

    void setTaskQueMaxThreshHold(int threshhold) {
        if (checkRunningState()) return;
        taskQueMaxThreshHold_ = threshhold;
    }

    void setThreadSizeThreshHold(int threshhold) {
        if (checkRunningState()) return;
        if (poolMode_ == PoolMode::MODE_CACHED) {
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
    auto submitTask(Task_Config config, Func &&func, Args &&... args)
    -> std::future<decltype(func(args...))> {
        using RType = decltype(func(args...));
        auto task_f = std::make_shared<std::packaged_task<RType()>>(
                std::bind(std::forward<Func>(func), std::forward<Args>(args)...));

        std::future<RType> result = task_f->get_future();
        std::unique_lock<std::mutex> lock(taskQueMtx_);
        if (!notFull_.wait_for(lock, std::chrono::seconds(1), [&]() -> bool {
            return taskQue_.size() < (size_t) taskQueMaxThreshHold_;
        })) {
            auto task_ = std::make_shared<std::packaged_task<RType()>>(
                    []() -> RType { return RType(); });
            //todo a task submit fail tips
            (*task_)();
            return task_->get_future();
        }
        auto task_func = [task_f]() { (*task_f)(); };
        taskQue_.emplace(task_func, config);
        taskSize_++;
        notEmpty_.notify_all();

        /*  std::cout << " taskSize_ =  " << taskSize_ << " idleThreadSize_ "
                   << idleThreadSize_ << "\n";
         std::cout << " curThreadSize_ =  " << curThreadSize_
                   << "  threadSizeThreshHold_ " << threadSizeThreshHold_
                   << "\n"; */
        if (poolMode_ == PoolMode::MODE_CACHED && taskSize_ > idleThreadSize_ &&

            curThreadSize_ < threadSizeThreshHold_) {
            /* std::cout << " ----------------------------------------------- "
                         "create new thread...\n"; */
            auto ptr = std::make_unique<Thread>([this](auto &&PH1) {
                threadFunc(std::forward<decltype(PH1)>(PH1));
            });
            int threadId = ptr->getId();
            threads_.emplace(threadId, std::move(ptr));
            threads_[threadId]->launch();
            curThreadSize_++;
            idleThreadSize_++;
        }

        return result;
    }

    void start(int initThreadSize = (int) std::thread::hardware_concurrency()) {
        isPoolRunning_ = true;
        initThreadSize_ = initThreadSize;
        idleThreadSize_ = initThreadSize_;
        curThreadSize_ = initThreadSize_;

        for (int i = 0; i < initThreadSize_; i++) {
            auto ptr = std::make_unique<Thread>([this](auto &&PH1) {
                threadFunc(std::forward<decltype(PH1)>(PH1));
            });
            int threadId = ptr->getId();
            threads_.emplace(threadId, std::move(ptr));
        }

        for (int i = 0; i < initThreadSize_; i++) {
            threads_[i]->launch();
            idleThreadSize_++;
        }
    }

    ThreadPool(const ThreadPool &) = delete;

    ThreadPool &operator=(const ThreadPool &) = delete;

private:
    /*
     * 定义线程函数, 此线程池没有管理线程
     * 每个线程函数 执行的时候就会判断是否需要线程数缩减
     * 而是否能增加线程数，是在submitTask哪里做的。
     */
    void threadFunc(int threadid) {
        for (;;) {
            Task task;
            {
                std::unique_lock<std::mutex> lock(taskQueMtx_);
//                std::cout << "tid:" << std::this_thread::get_id()
//                          << "try to get task ..." << std::endl;
                while (taskQue_.empty()) {
                    if (!isPoolRunning_) {
                        threads_.erase(threadid);  // std::this_thread::getid()
//                        std::cout << "threadid:" << std::this_thread::get_id()
//                                  << " exit!" << std::endl;
                        exitCond_.notify_all();
                        return;
                    }
                    if (poolMode_ == PoolMode::MODE_CACHED) {
                        if (std::cv_status::timeout ==
                            notEmpty_.wait_for(lock, std::chrono::seconds(1))) {
                            auto now =
                                    std::chrono::high_resolution_clock::now();
                            auto dur = std::chrono::duration_cast<
                                    std::chrono::seconds>(now - lastTime);
                            if (dur.count() >= THREAD_MAX_IDLE_TIME_S &&
                                curThreadSize_ > initThreadSize_) {
                                threads_.erase(threadid);
                                curThreadSize_--;
                                idleThreadSize_--;
//                                std::cout
//                                        << "threadid:" << std::this_thread::get_id()
//                                        << " exit!" << std::endl;
                                return;
                            }
                        }
                    } else {
                        notEmpty_.wait(lock);
                    }
                }

                idleThreadSize_--;
//                std::cout << "tid:" << std::this_thread::get_id()
//                          << "get task..." << std::endl;
                task = std::move(taskQue_.front());
                // 设置CPU核心
                if (task.config_.num_cores_ < NUM_CORES &&
                    task.config_.num_cores_ > 0) {
                    CPU_ZERO(&cpuset);
                    for (int i = 0; i < task.config_.num_cores_; ++i) {
                        CPU_SET(i, &cpuset);
                    }
                    if (pthread_setaffinity_np(
                            pthread_self(), sizeof(cpu_set_t), &cpuset) < 0) {
                    }
                }
                // 设置优先级
                struct sched_param param{};
                param.sched_priority = task.config_.priority_;
                if (pthread_setschedparam(pthread_self(), task.config_.mode_,
                                          &param) > 0) {
                    std::cerr << "some error \n";
                    // todo throw exception
                }

                taskQue_.pop();
                taskSize_--;
                if (!taskQue_.empty()) {
                    notEmpty_.notify_all();
                }
                notFull_.notify_all();
            }
            if (task.task_func != nullptr) {
                task.task_func();
            }
            idleThreadSize_++;
            lastTime = std::chrono::high_resolution_clock::now();
        }
    }

    bool checkRunningState() const { return isPoolRunning_; }

    class Task {
    public:
        using task_f = std::function<void()>;

        Task(task_f f = nullptr, Task_Config config = {})
                : task_func(std::move(f)), config_(config) {}

        task_f task_func = nullptr;
        Task_Config config_;

    private:
    };

    std::unordered_map<int, std::unique_ptr<Thread>> threads_;

    int initThreadSize_;
    int threadSizeThreshHold_;
    std::atomic_int curThreadSize_;
    std::atomic_int idleThreadSize_;

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
