#include "EventLoop.hpp"
#include "Channel.hpp"
#include <iostream>
#include <sys/epoll.h>
#include <unistd.h> // 提供 close 函数

namespace MyServer
{
    EventLoop::EventLoop() : epfd_(-1), quit_(false), activeEvents_(1024) {
        // 创建 epoll 实例
        epfd_ = epoll_create1(0);
        if (epfd_ == -1) {
            std::cerr << "Epoll 创建失败!" << std::endl;
            exit(EXIT_FAILURE);
        }
    }

    EventLoop::~EventLoop() {
        if (epfd_ != -1) {
            close(epfd_);
        }
    }

    void EventLoop::loop() {
        while (!quit_) {
            int nfds = epoll_wait(epfd_, activeEvents_.data(), static_cast<int>(activeEvents_.size()), -1);
            if (nfds == -1) {
                std::cerr << "Epoll 等待事件失败!" << std::endl;
                continue; // 出错了，但我们不想退出整个服务器，所以继续循环
            }

            for (int i = 0; i < nfds; ++i) {
                Channel* channel = static_cast<Channel*>(activeEvents_[i].data.ptr);
                channel->setRevents(activeEvents_[i].events); // 把实际发生的事件告诉 Channel
                channel->handleEvent(); // 让 Channel 自己去处理事件
            }
        }
    }

    void EventLoop::updateChannel(Channel* channel) {
        int fd = channel->getFd();
        uint32_t events = channel->getEvents();

        struct epoll_event ev;
        ev.data.ptr = channel; 
        // 把 Channel 的指针存到 epoll_event 的data.ptr 中，这样 epoll_wait 醒来时就能找到对应的 Channel 对象
        ev.events = events; // 关注 Channel 想监听的事件

        if (epoll_ctl(epfd_, EPOLL_CTL_MOD, fd, &ev) == -1) {
            if (errno == ENOENT) {
                // 如果是因为这个 fd 还没有被注册过，那么就添加它
                if (epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &ev) == -1) {
                    std::cerr << "Epoll 添加 Channel 失败!" << std::endl;
                }
            } else {
                std::cerr << "Epoll 修改 Channel 失败!" << std::endl;
            }
        }
    }
}