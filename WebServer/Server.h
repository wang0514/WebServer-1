// @Author Lin Ya
// @Email xxbbb@vip.qq.com
#pragma once
#include "EventLoop.h"
#include "Channel.h"
#include "EventLoopThreadPool.h"
#include <memory>


class Server
{
public:
    Server(EventLoop *loop, int threadNum, int port);
    ~Server() { }
    EventLoop* getLoop() const { return loop_; }
    void start();
    void handNewConn();
    void handThisConn() { loop_->updatePoller(acceptChannel_); }//处理当前文件描述符

private:
    EventLoop *loop_;//eventloop对象的指针
    int threadNum_;//线程的数量
    std::unique_ptr<EventLoopThreadPool> eventLoopThreadPool_;//eventloop线程池
    bool started_;
    std::shared_ptr<Channel> acceptChannel_; //接受事件
    int port_;//端口号
    int listenFd_;//监听文件描述符
    static const int MAXFDS = 100000;//最大文件文件描述付个数
};