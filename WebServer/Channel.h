// @Author Lin Ya
// @Email xxbbb@vip.qq.com
#pragma once
#include "Timer.h"
#include <string>
#include <unordered_map>
#include <memory>
#include <sys/epoll.h>
#include <functional>
#include <sys/epoll.h>


class EventLoop;
class HttpData;


class Channel
{
private:
    typedef std::function<void()> CallBack;//回调函数对象类型
    EventLoop *loop_;//eventloop对象的指针
    int fd_;//文件描述符
    __uint32_t events_;//关注的事件类型
    __uint32_t revents_;//当前活动的事件
    __uint32_t lastEvents_;

    // 方便找到上层持有该Channel的对象
    std::weak_ptr<HttpData> holder_;

private:
    int parse_URI();
    int parse_Headers();
    int analysisRequest();

    //回调函数
    CallBack readHandler_;
    CallBack writeHandler_;
    CallBack errorHandler_;
    CallBack connHandler_;

public:
    //两个构造函数
    Channel(EventLoop *loop);
    Channel(EventLoop *loop, int fd);
    ~Channel();
    int getFd();
    void setFd(int fd);

    void setHolder(std::shared_ptr<HttpData> holder)//TODO
    {
        holder_ = holder;
    }
    std::shared_ptr<HttpData> getHolder()//TODO
    {
        std::shared_ptr<HttpData> ret(holder_.lock());
        return ret;
    }

    void setReadHandler(CallBack&& readHandler)
    {
        readHandler_ = readHandler;
    }
    void setWriteHandler(CallBack&& writeHandler)
    {
        writeHandler_ = writeHandler;
    }
    void setErrorHandler(CallBack&& errorHandler)
    {
        errorHandler_ = errorHandler;
    }
    void setConnHandler(CallBack&& connHandler)
    {
        connHandler_ = connHandler;
    }
/**
 * 整个channel类最关键的部分
 * 根据revents_的值分别调用不同的用户回调
 */
    void handleEvents()
    {
        events_ = 0;
        /**
         * EPOLLHUP：这个好像有些系统检测不到，可以使用EPOLLIN，read返回0，删除掉事件，关闭close(fd);
         * 如果有EPOLLRDHUP，检测它就可以知道是对方关闭；否则就用上面方法。
         * 此时应该是对方关闭客户端连接，且当前事件没有数据要读
         * 直接返回
         */
        if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN))
        {
            events_ = 0;
            return;
        }
        /**
         * error处理函数，检测到error，回调errorHandler_()
         */
        if (revents_ & EPOLLERR)
        {
            if (errorHandler_)
                errorHandler_();
            events_ = 0;
            return;
        }
        /**
         * 检测到数据到达或者新连接，或者外带数据，或者检测到对方关闭
         */
        if (revents_ & (EPOLLIN | EPOLLPRI | EPOLLRDHUP))
        {
            handleRead();
        }
        /**
         * 检测到有数据要写
         * 调用handlewrite（）回调
         */
        if (revents_ & EPOLLOUT)
        {
            handleWrite();
        }
        /**
         * 处理新连接
         * 因为前面是监听时调用这个函数，所以必须进行新连接的监听
         */
        handleConn();
    }
    void handleRead();
    void handleWrite();
    void handleError(int fd, int err_num, std::string short_msg);
    void handleConn();

    void setRevents(__uint32_t ev)
    {
        revents_ = ev;
    }

    void setEvents(__uint32_t ev)
    {
        events_ = ev;
    }
    __uint32_t& getEvents()
    {
        return events_;
    }

    bool EqualAndUpdateLastEvents()
    {
        bool ret = (lastEvents_ == events_);
        lastEvents_ = events_;
        return ret;
    }

    __uint32_t getLastEvents()
    {
        return lastEvents_;
    }

};

typedef std::shared_ptr<Channel> SP_Channel;