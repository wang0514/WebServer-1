// @Author Lin Ya
// @Email xxbbb@vip.qq.com
#include "Server.h"
#include "base/Logging.h"
#include "Util.h"
#include <functional>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

/**
 * server的构造函数
 * @param loop ：eventloop
 * @param threadNum ：线程数量
 * @param port ：端口号
 */
Server::Server(EventLoop *loop, int threadNum, int port)
:   loop_(loop),
    threadNum_(threadNum),
    eventLoopThreadPool_(new EventLoopThreadPool(loop_, threadNum)),
    started_(false),
    acceptChannel_(new Channel(loop_)),
    port_(port),
    listenFd_(socket_bind_listen(port_))
{
    acceptChannel_->setFd(listenFd_);
    handle_for_sigpipe();
    if (setSocketNonBlocking(listenFd_) < 0)
    {
        perror("set socket non block failed");
        abort();
    }
}

/**
 * server端开启操作
 */
void Server::start()
{
    /**
     * 开启线程池的操作，生成threadNum个线程，每个线程在EventLoopThread对象中，
     * 且有一个Thread数组和一个EventLoop数组存储这些内容
     * 每个创建好的线程都在处于pooler状态，且初始化的线程只有对应的EventFd描述符
     */
    eventLoopThreadPool_->start();
    //acceptChannel_->setEvents(EPOLLIN | EPOLLET | EPOLLONESHOT);
    acceptChannel_->setEvents(EPOLLIN | EPOLLET);//设置epoll为ET模式
    acceptChannel_->setReadHandler(bind(&Server::handNewConn, this));//注册acceptChannel_处理新连接函数
    acceptChannel_->setConnHandler(bind(&Server::handThisConn, this));//注册acceptChannel_处理当前连接函数
    loop_->addToPoller(acceptChannel_, 0);//将acceptChannel加入epoll检测，这是mainReactor
    started_ = true;//设置开启标志
}

/**
 * server处理新的连接
 */
void Server::handNewConn()
{
    //client的地址信息，需要先声明，然后接受时传进去
    struct sockaddr_in client_addr;
    memset(&client_addr, 0, sizeof(struct sockaddr_in));
    socklen_t client_addr_len = sizeof(client_addr);
    int accept_fd = 0;
    while((accept_fd = accept(listenFd_, (struct sockaddr*)&client_addr, &client_addr_len)) > 0)
    {
        //轮询法确定监听到的文件描述符交给哪一个EventLoopThread处理
        EventLoop *loop = eventLoopThreadPool_->getNextLoop();
        LOG << "New connection from " << inet_ntoa(client_addr.sin_addr) << ":" << ntohs(client_addr.sin_port);
        // cout << "new connection" << endl;
        // cout << inet_ntoa(client_addr.sin_addr) << endl;
        // cout << ntohs(client_addr.sin_port) << endl;
        /*
        // TCP的保活机制默认是关闭的
        int optval = 0;
        socklen_t len_optval = 4;
        getsockopt(accept_fd, SOL_SOCKET,  SO_KEEPALIVE, &optval, &len_optval);
        cout << "optval ==" << optval << endl;
        */
        // 限制服务器的最大并发连接数
        if (accept_fd >= MAXFDS)
        {
            close(accept_fd);
            continue;
        }
        // 设为非阻塞模式
        if (setSocketNonBlocking(accept_fd) < 0)
        {
            LOG << "Set non block failed!";
            //perror("Set non block failed!");
            return;
        }

        //禁用 Nagle’s Algorithm
        setSocketNodelay(accept_fd);
        //setSocketNoLinger(accept_fd);

        shared_ptr<HttpData> req_info(new HttpData(loop, accept_fd));//TODO
        req_info->getChannel()->setHolder(req_info);//TODO
        /**
         * 跨线程调用，也就是主Reactor将新任务添加到SubReactor中进行处理，这里是一个异步调用
         * 因为不是当前线程调用这个处理函数，queueInLoop会将这个函数添加到pendingFunctors_队列
         * 紧接着唤醒loop所在EventLoop，也就是往loop对应的EventLoop对象中的EventFd写一个byte的数据
         */
        loop->queueInLoop(std::bind(&HttpData::newEvent, req_info));
    }
    acceptChannel_->setEvents(EPOLLIN | EPOLLET);
}