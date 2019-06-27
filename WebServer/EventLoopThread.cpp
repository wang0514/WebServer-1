// @Author Lin Ya
// @Email xxbbb@vip.qq.com
#include "EventLoopThread.h"
#include <functional>

/**
 * EventLoopThread的构造函数
 */
EventLoopThread::EventLoopThread()
:   loop_(NULL),
    exiting_(false),
    thread_(bind(&EventLoopThread::threadFunc, this), "EventLoopThread"),
    mutex_(),
    cond_(mutex_)//cond初始化时带了一个锁
{}

EventLoopThread::~EventLoopThread()
{
    exiting_ = true;
    if (loop_ != NULL)
    {
        loop_->quit();
        thread_.join();//回收子进程
    }
}

EventLoop* EventLoopThread::startLoop()
{
    assert(!thread_.started());
    /**
     *就是创建线程，然后启动线程，启动线程的入口函数被注册为threadFunc
     * threadFunc中执行loop->loop()函数，也就是在循环监听，
     * 此时这个subReactor中只有对应的EventFd唤醒文件描述符
     */
    thread_.start();
    {
        MutexLockGuard lock(mutex_);//这是一个MutexLockGuard的类的拷贝构造函数
        // 一直等到threadFun在Thread里真正跑起来
        while (loop_ == NULL)
            cond_.wait();
        /**
         * 为什么这里不需要再加上一个关闭锁的操作，因为这里的锁是用类生成的
         * 只在这个函数作用域里面存在
         * 当函数返回时，会自动调用这个类的析构函数
         * 会把这个类和他的基类都析构，也就释放锁，并且销毁锁了
         */
    }
    return loop_;
}

/**
 * EventLoopThread处理函数，
 */
void EventLoopThread::threadFunc()
{
    EventLoop loop;

    {
        MutexLockGuard lock(mutex_);//这是一个MutexLockGuard的类的拷贝构造函数
        loop_ = &loop;
        cond_.notify();
    }

    loop.loop();//TODO maybe是从监听到就绪队列的第一个生产者-消费者模型
    //assert(exiting_);
    loop_ = NULL;
}