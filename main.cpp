//
// Created by 10447 on 2023-09-05.
//

#include "threadpool.h"

using uLong = unsigned  long long;
uLong run(int b ,int e)  {
        uLong temp = 0;
        for (uLong i = b; i <= e; ++i) {
            temp += i;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
        return temp;
    }
int main(){
    ThreadPool p;
    p.setMode(PoolMode::MODE_CACHED);
    p.start(2);
    auto res = p.submitTask(run,1,100000000);
    std::cout <<  res .get() <<std:: endl;
    return 0;
}
