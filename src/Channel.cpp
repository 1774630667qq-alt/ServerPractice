#include <iostream>
#include "Channel.hpp"
#include "EventLoop.hpp"
#include <sys/epoll.h>
#include <unistd.h> // 提供 close 函数

namespace MyServer {
    Channel::Channel(EventLoop* loop, int fd) : loop_(loop), fd_(fd), events_(0), revents_(0) {}

    void Channel::handleEvent() {
        if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)) {
            // 连接被挂断了，并且没有可读事件了，说明是对方关闭了连接
            if (closeCallback_) {
                closeCallback_();
            }
        } 

        if (revents_ & EPOLLERR) {
            // 发生错误了
            if (closeCallback_) {
                closeCallback_();
            }
        }

        if (revents_ & (EPOLLIN | EPOLLPRI | EPOLLRDHUP)) {
            if (readCallback_) {
                readCallback_();
            }
        }
    }

    void Channel::enableReading() {
        events_ |= EPOLLIN;
        loop_->updateChannel(this);
    }

    
}