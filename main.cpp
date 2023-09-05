//
// Created by 10447 on 2023-09-05.
//
#include "iostream"
#include "threadpool.h"

int main() {
    Threadpool p;
    p.start(10);
    getchar();
    return 0;
}