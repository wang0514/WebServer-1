// @Author Lin Ya
// @Email xxbbb@vip.qq.com
#include "EventLoop.h"
#include "base/Logging.h"
#include "Util.h"
#include <sys/eventfd.h>
#include <sys/epoll.h>
#include <iostream>
using namespace std;

__thread EventLoop* t_loopInThisThread = 0;

int createEventfd()
{

//    eventfd()创建一个“eventfd对象”，这个对象能被用户空间应用用作一个事件等待/响应机制，靠内核去响应用户空间应用事件。
//    这个对象包含一个由内核保持的无符号64位整型计数器。这个计数器由参数initval说明的值来初始化。
//    它的标记可以有以下属性：
//    EFD_CLOEXEC：FD_CLOEXEC，简单说就是fork子进程时不继承，对于多线程的程序设上这个值不会有错的。
//    EFD_NONBLOCK：文件会被设置成O_NONBLOCK，一般要设置。
//    EFD_SEMAPHORE：（2.6.30以后支持）支持semophore语义的read，简单说就值递减1。
    int evtfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0)
    {
        LOG << "Failed in eventfd";
        abort();
    }
    return evtfd;
}

EventLoop::EventLoop()//EventLoop的构造函数
:   looping_(false),
    poller_(new Epoll()),
    wakeupFd_(createEventfd()),
    quit_(false),
    eventHandling_(false),
    callingPendingFunctors_(false),
    threadId_(CurrentThread::tid()),
    pwakeupChannel_(new Channel(this, wakeupFd_))
{
    if (t_loopInThisThread)
    {
        //LOG << "Another EventLoop " << t_loopInThisThread << " exists in this thread " << threadId_;
    }
    else
    {
        t_loopInThisThread = this;
    }
    //pwakeupChannel_->setEvents(EPOLLIN | EPOLLET | EPOLLONESHOT);
    pwakeupChannel_->setEvents(EPOLLIN | EPOLLET);
    pwakeupChannel_->setReadHandler(bind(&EventLoop::handleRead, this));
    pwakeupChannel_->setConnHandler(bind(&EventLoop::handleConn, this));
    poller_->epoll_add(pwakeupChannel_, 0);
}

void EventLoop::handleConn()
{
    //poller_->epoll_mod(wakeupFd_, pwakeupChannel_, (EPOLLIN | EPOLLET | EPOLLONESHOT), 0);
    updatePoller(pwakeupChannel_, 0);
}

/**
 * 关闭建立的epollfd，并且设置当前t_loopInThisThread为0，也就是当前线程没有eventloop对象
 */
EventLoop::~EventLoop()
{
    //wakeupChannel_->disableAll();
    //wakeupChannel_->remove();
    close(wakeupFd_);
    t_loopInThisThread = NULL;
}

//因为epollfd为1时是写描述符，所以写1
void EventLoop::wakeup()//TODO 怎么个唤醒方法，感觉像是往epollfd里写数据，
{
    uint64_t one = 1;
    ssize_t n = writen(wakeupFd_, (char*)(&one), sizeof one);
    if (n != sizeof one)
    {
        LOG<< "EventLoop::wakeup() writes " << n << " bytes instead of 8";
    }
}

void EventLoop::handleRead()
{
    uint64_t one = 1;
    ssize_t n = readn(wakeupFd_, &one, sizeof one);
    if (n != sizeof one)
    {
        LOG << "EventLoop::handleRead() reads " << n << " bytes instead of 8";
    }
    //pwakeupChannel_->setEvents(EPOLLIN | EPOLLET | EPOLLONESHOT);
    pwakeupChannel_->setEvents(EPOLLIN | EPOLLET);
}

/**
 * 事件循环调用的回调函数
 * @param cb
 */
void EventLoop::runInLoop(Functor&& cb)
{
    //只在线程里只有一个事件时进行回调
    // 当前线程是ower线程则立即执行，否则放到ower线程的任务队列，异步执行
    if (isInLoopThread())
        cb();
    else
        queueInLoop(std::move(cb));
}

/**
 * 如果时EventLoop的owner线程，会调用runInLoop会立即执行回调函数cb
 * 否则会把回调函数放到任务队列（其实时vector），即调用queueInLoop函数。
 * 如果不是当前线程调用，或者正在执行pendingFunctors_中的任务，
 * 都要唤醒EventLoop的owner线程，让其执行pendingFunctors_中的任务。
 * 如果正在执行pendingFunctors_中的任务，添加新任务后不会执行新的任务，
 * 因为functors.swap(pendingFunctors_)后，执行的时functors中的任务
 * @param cb
 */
void EventLoop::queueInLoop(Functor&& cb)
{
    {
        MutexLockGuard lock(mutex_);//TODO 需要理解
        //emplace_back和push_back都是向容器内添加数据.
        //对于在容器中添加类的对象时, 相比于push_back,emplace_back可以避免额外类的复制和移动操作.
        pendingFunctors_.emplace_back(std::move(cb));
    }

    if (!isInLoopThread() || callingPendingFunctors_)
        wakeup();
}

/**
 * 事件的循环函数
 */
void EventLoop::loop()
{
    assert(!looping_);
    assert(isInLoopThread());
    looping_ = true;//开始循环
    quit_ = false;//退出标志设为0
    //LOG_TRACE << "EventLoop " << this << " start looping";
    std::vector<SP_Channel> ret;//一个channel类对象的指针集合
    while (!quit_)
    {
        //cout << "doing" << endl;
        ret.clear();//清空ret集合
        ret = poller_->poll();//阻塞监听
        eventHandling_ = true;//监听到了，事件处理标志设为true，开始处理事件
        for (auto &it : ret)//循环处理监听到的事件
            it->handleEvents();//根据对应的epoll参数的不同调用不同的处理函数
        eventHandling_ = false;//处理完了后将事件处理标志设为1
        doPendingFunctors();//TODO 暂时没看懂这个函数要干什么
        poller_->handleExpired();//TODO 待解析
    }
    looping_ = false;//关掉loop循环标志
}

void EventLoop::doPendingFunctors()
{
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;

    {
        MutexLockGuard lock(mutex_);
        functors.swap(pendingFunctors_);
    }

    for (size_t i = 0; i < functors.size(); ++i)
        functors[i]();
    callingPendingFunctors_ = false;
}

/**
 * epollevent退出函数
 */
void EventLoop::quit()
{
    quit_ = true;//将退出标志设为true
    if (!isInLoopThread())//事件没有循环，直接唤醒epollevent
    {
        wakeup();
    }
}