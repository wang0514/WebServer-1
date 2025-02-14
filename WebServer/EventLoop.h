// @Author Lin Ya
// @Email xxbbb@vip.qq.com
#pragma once
#include "base/Thread.h"
#include "Epoll.h"
#include "base/Logging.h"
#include "Channel.h"
#include "base/CurrentThread.h"
#include "Util.h"
#include <vector>
#include <memory>
#include <functional>

#include <iostream>
using namespace std;


class EventLoop
{
public:
    typedef std::function<void()> Functor;
    EventLoop();
    ~EventLoop();
    void loop();
    void quit();
    void runInLoop(Functor&& cb);
    void queueInLoop(Functor&& cb);
    bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }
    void assertInLoopThread()//确认是不是当前线程，即满足one thread one loop条件
    {
        assert(isInLoopThread());
    }
    void shutdown(shared_ptr<Channel> channel)
    {
        shutDownWR(channel->getFd());
    }
    void removeFromPoller(shared_ptr<Channel> channel)
    {
        //shutDownWR(channel->getFd());
        poller_->epoll_del(channel);
    }
    void updatePoller(shared_ptr<Channel> channel, int timeout = 0)
    {
        poller_->epoll_mod(channel, timeout);
    }
    void addToPoller(shared_ptr<Channel> channel, int timeout = 0)
    {
        poller_->epoll_add(channel, timeout);
    }
    
private:
    // 声明顺序 wakeupFd_ > pwakeupChannel_
    bool looping_;//判断当前是否有循环，one thread one loop
    shared_ptr<Epoll> poller_;//epoll事件
    int wakeupFd_;
    bool quit_;//退出标志
    bool eventHandling_;//是否正在处理event事件
    mutable MutexLock mutex_;
    std::vector<Functor> pendingFunctors_;
    bool callingPendingFunctors_;
    const pid_t threadId_; 
    shared_ptr<Channel> pwakeupChannel_;
    
    void wakeup();
    void handleRead();
    void doPendingFunctors();
    void handleConn();
};
