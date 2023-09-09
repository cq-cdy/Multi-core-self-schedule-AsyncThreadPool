# 设置CMake的最小版本要求
cmake_minimum_required(VERSION 3.22)

# 设置项目名称和版本
project(thread_pool)
# 设置C++标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
# 添加源文件
add_executable(${PROJECT_NAME} main.cpp
        threadpool.cpp
        myallocator.h
        semaphore.h
)

# 找到需要的库


# 链接库文件到可执行文件
