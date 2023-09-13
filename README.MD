##  可变参函数模板Linux高性能异步线程池
* __1.可针对任务类型(如IO密集型任务和CPU密集任务)及任务重要性对线程的CPU核数绑定，调整操作系统对用户任意任务的调度策略与优先级__
* __2.提供可变参函数模板，使用简单高效__
* __3.提供固定线程模式以及实时增减线程__
* __4.用户可异步获取任务函数返回值__
----------------

## How to use the thread_pool? 

-------------

### 一些配基本配置
* 引入头文件 
``` C++
#include "threadpool.h"
```
* 实例化线程池对象
``` C++
 ThreadPool pool;
```
* 一些基本属性设置
``` C++
 pool.setTaskQueMaxThreshHold(4096) // 设置最大任务数量是66个，默认20000

 pool.setThreadSizeThreshHold(100); // 设置最大线程数量是100个，默认1024
```
* 线程池工作模式的设置

> FIXED 模式(默认)
>> 默认保持线程池初始线程，不会动态扩容收缩线程。

>CACHED 模式
>> 根据任务数量动态更新线程数量,保持在最大线程
```
pool.setMode(PoolMode::MODE_CACHED);
```
* 启动线程池
> 可传入初始线程数量，不传入默认为cpu的核心数

``` C++
pool.start(); // 默认初始线程为CPU核心数
pool.start(10); // 指定初始线程数为10；
```
----- 
### 提交任务
```
template <typename Func, typename... Args>
pool.submitTask(Task_Config config, Func&& func, Args&&... args)
```
>```Task_Config(int num_cores=-1, int priority =SCHED_OTHER, int mode= 99)```
>> ```num_cores :``` 该任务所需要的CPU核心数，默认为操作系统自动调度 , 
>>
>>```priority:```该任务的调度策略，默认采用操作系统自动调度
>>> _```priority```还可以指定如:```SCHED_FIFO,SCHED_ISO,SCHED_BATCH```等调度模式_
>>
>> ```mode :```  任务被调度的优先级，数值越大优先级越高
>>
>关于任务优先级设置可参考[[UNIX 相关API文档](https://www.man7.org/linux/man-pages/man7/sched.7.html)]
>> ```Func 可传入任意任务函数```
>
>>```Args :Func函数的参数列表```
> 


``` c++
//你的函数
unsigned int task_fun(int b,int n){
    unsigned int res = 0;
    for(int i = b ; i < n ; ++i){
        res +=i;
    }
    return res;
}
//提交第一个任务，'{}'表示CPU核心数和调度策略采用磨操作系统默认
auto res = pool.submitTask({},task_fun,1,100000); 

/*提交第二个任务，并设置CPU核心数为2,采用FIFO调度方式,优先级数值为90*/
auto res2 = pool.submitTask({2,SCHED_FIFO,90},task_fun,1,100000);

pool.submitTask({6,SCHED_FIFO,50},[](){
    do_something();
});
```
------
### 异步获取任务返回值
> ```get()```函数获取返回值，若线程线程函数执行完毕，则直接获取返回值，否则阻塞等待
```c++
std::cout << "the result1 is :" << res.get(); << std::endl;
std::cout << "the result2 is :" << res2.get(); << std::endl;
```
------
### 后续
* 后续会加入日志系统

