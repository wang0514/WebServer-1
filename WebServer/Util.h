// @Author Lin Ya
// @Email xxbbb@vip.qq.com
#pragma once
#include <cstdlib>
#include <string>

/**
 * 整个项目的一些处理函数都放在这里
 * @param fd
 * @param buff
 * @param n
 * @return
 */
ssize_t readn(int fd, void *buff, size_t n);
ssize_t readn(int fd, std::string &inBuffer, bool &zero);
ssize_t readn(int fd, std::string &inBuffer);
ssize_t writen(int fd, void *buff, size_t n);
ssize_t writen(int fd, std::string &sbuff);
void handle_for_sigpipe();
int setSocketNonBlocking(int fd);
void setSocketNodelay(int fd);
void setSocketNoLinger(int fd);
void shutDownWR(int fd);
int socket_bind_listen(int port);