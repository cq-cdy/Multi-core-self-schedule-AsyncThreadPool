//
// Created by 10447 on 2023-09-05.
//
#include "iostream"
#include "threadpool.h"
#include "myallocator.h"
#include "string"
#include "typeinfo"

using uLong =  unsigned  long long ;
class mytask : public Task {
public:
    uLong  b ,e;
    mytask(uLong begin,uLong end){
        b = begin;
        e = end;
    }
    Any run() override {
        uLong  temp= 0;
        for (uLong i = b;i <= e; ++i){
            temp += i;
        }
        return {temp};
    }
};


int main(){
    Threadpool p;
    p.setTaskQuemaskThreshHold(10);
    p.start(10);
    std::shared_ptr<Task> t(new mytask(1,100000000));
    std::shared_ptr<Task> t1(new mytask(100000001,200000000));
    std::shared_ptr<Task> t2(new mytask(200000001,300000000));
    auto r1 = p.submitTask(t);
    auto r2 = p.submitTask(t1);
    auto r3 = p.submitTask(t2);
   std::cout << r1.get().cast<uLong>() +r2.get().cast<uLong>() + r3.get().cast<uLong>();
}

#if 0
#include "memory"

using namespace std;

class A {
public:
    A() = default;

    A(const A &) = delete;

    A(A &) = delete;

    A(const A &&data) noexcept {
        cout << "move" << endl;
    }

    A(A &&data) noexcept {
        cout << "move" << endl;
    }
};

A test() {
    A a;
    cout << &a << endl;
    return a;// 这里返回的是 编译器优化，并不是 发生了移动构造
}

unique_ptr<string> func(string &data) {
    cout << &data << endl;
    unique_ptr<string> ptr(&data);
    cout << &ptr << endl;
    return ptr; // 这里返回的是 编译器优化，并不是 发生了移动构造
}

A &&test2() {
    A a;
    cout << &a << endl;
    return std::move(a);// 这里返回的是 编译器优化，并不是 发生了移动构造
}

class MyObject {
public:
    int a = 9;
    MyObject() = default;

    MyObject(const MyObject &) = delete; // 禁用拷贝构造函数

    MyObject(MyObject &&other) noexcept {
        std::cout << "Move constructor called" << std::endl;
    }
};

MyObject &&test(MyObject &data) {
    std::cout << "test called" << std::endl;
    return std::move(data);
}

int other() {
    MyObject obj1;
    std::cout << "obj1 . a: " <<obj1.a << std::endl;
    MyObject &&obj2 = std::move(obj1);
    std::cout << "obj1 . a: " <<obj1.a << std::endl;

    std::cout << "Address of obj1: " << &obj1 << std::endl;
    std::cout << "Address of obj2: " << &obj2 << std::endl;
    MyObject &&obj3 = test(obj1);
    std::cout << "Address of obj3: " << &obj3 << std::endl;
    auto obj4 = test(obj1); // 发生 移动构造
    std::cout << "Address of obj4: " << &obj4 << std::endl;


//    cout <<&s <<endl;
//   auto x =  func(s);
//    cout << &x<< endl;
    // func(s);

    return 0;
}

#endif