// @Author Lin Ya
// @Email xxbbb@vip.qq.com
#pragma once
#include "Timer.h"
#include <string>
#include <unordered_map>
#include <map>
#include <memory>
#include <sys/epoll.h>
#include <functional>
#include <unistd.h>

class EventLoop;
class TimerNode;
class Channel;

/**
 * 枚举处理状态
 */
enum ProcessState
{
    STATE_PARSE_URI = 1,
    STATE_PARSE_HEADERS,
    STATE_RECV_BODY,
    STATE_ANALYSIS,
    STATE_FINISH
};

/**
 * 枚举URL状态
 */
enum URIState
{
    PARSE_URI_AGAIN = 1,
    PARSE_URI_ERROR,
    PARSE_URI_SUCCESS,
};


enum HeaderState
{
    PARSE_HEADER_SUCCESS = 1,
    PARSE_HEADER_AGAIN,
    PARSE_HEADER_ERROR
};

enum AnalysisState
{
    ANALYSIS_SUCCESS = 1,
    ANALYSIS_ERROR
};

enum ParseState
{
    H_START = 0,
    H_KEY,
    H_COLON,
    H_SPACES_AFTER_COLON,
    H_VALUE,
    H_CR,
    H_LF,
    H_END_CR,
    H_END_LF
};

enum ConnectionState
{
    H_CONNECTED = 0,
    H_DISCONNECTING,
    H_DISCONNECTED    
};

enum HttpMethod
{
    METHOD_POST = 1,
    METHOD_GET,
    METHOD_HEAD
};

enum HttpVersion
{
    HTTP_10 = 1,
    HTTP_11
};

/**
 * 类：用于描述消息内容类型的因特网标准
 */
class MimeType
{
private:
    static void init();
    //使用unordered_map存储消息内容类型
    static std::unordered_map<std::string, std::string> mime;
    MimeType();//构造函数
    MimeType(const MimeType &m);//拷贝构造函数

public:
    static std::string getMime(const std::string &suffix);

private:
    static pthread_once_t once_control;
};


class HttpData: public std::enable_shared_from_this<HttpData>
{
public:
    HttpData(EventLoop *loop, int connfd);
    ~HttpData() { close(fd_); }
    void reset();
    void seperateTimer();
    void linkTimer(std::shared_ptr<TimerNode> mtimer)
    {
        // shared_ptr重载了bool, 但weak_ptr没有
        timer_ = mtimer; 
    }
    std::shared_ptr<Channel> getChannel() { return channel_; }
    EventLoop *getLoop() { return loop_; }
    void handleClose();
    void newEvent();

private:
    EventLoop *loop_; //EventLoop事件指针
    std::shared_ptr<Channel> channel_;//Channel指针
    int fd_;//文件描述符
    std::string inBuffer_;//缓冲区
    std::string outBuffer_;//缓冲区
    bool error_;//是否出错的标志
    ConnectionState connectionState_;//枚举连接的状态

    HttpMethod method_;//http请求方法枚举
    HttpVersion HTTPVersion_;//http版本枚举
    std::string fileName_;//请求的文件名
    std::string path_;//请求的文件路径
    int nowReadPos_;//TODO
    ProcessState state_;//处理状态
    ParseState hState_;//分割状态
    bool keepAlive_;//是否保持长连接
    std::map<std::string, std::string> headers_;//头部信息的map
    std::weak_ptr<TimerNode> timer_;//计时器指针

    void handleRead();
    void handleWrite();
    void handleConn();
    void handleError(int fd, int err_num, std::string short_msg);
    URIState parseURI();
    HeaderState parseHeaders();
    AnalysisState analysisRequest();
};