
#include "threadpool.h"

using uLong = unsigned long long;

uLong run(int b, int e) {
    for (int j = 0; j < 15; j++) {
        uLong temp = 0;
        for (uLong i = b; i <= e; ++i) {
            temp += i;
        }
    }
    return 0;
}

int main() {
    ThreadPool pool;
    pool.setMode(PoolMode::MODE_CACHED);
    pool.setTaskQueMaxThreshHold(10000);
    pool.setThreadSizeThreshHold(100);
    pool.start(6);
    auto r = pool.submitTask({8, SCHED_FIFO, 99}, run, 1, 100000000);
    std::cout << r.get() << std::endl;
    return 0;
}
