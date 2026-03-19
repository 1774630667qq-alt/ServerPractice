/*
 * @Author: Zhang YuHua 1774630667@qq.com
 * @Date: 2026-03-19 15:20:10
 * @LastEditors: Zhang YuHua 1774630667@qq.com
 * @LastEditTime: 2026-03-19 15:46:55
 * @FilePath: /ServerPractice/include/EventLoop.hpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#pragma once
#include <vector>
#include <sys/epoll.h>

namespace MyServer {

class Channel; // 前向声明

/**
 * @brief 事件循环类 (Reactor 模式的发动机)
 * * 它的核心职责是：拥有并管理一个底层的 epoll 实例。
 * 它在一个死循环中不断调用 epoll_wait，一旦有事件发生，
 * 就找出对应触发事件的 Channel，并让 Channel 自己去处理事件。
 * * 它是完全不知道具体的业务逻辑的，也不知道网络报文是什么格式。
 */
class EventLoop {
private:
    int epfd_;      ///< 底层的 epoll 文件描述符
    bool quit_;     ///< 控制死循环是否退出的标志

    // 这是一个预先分配好大小的数组，用来接收 epoll_wait 返回的活跃事件
    std::vector<struct epoll_event> activeEvents_; 

public:
    /**
     * @brief 构造函数：负责调用 epoll_create1 初始化 epfd_
     */
    EventLoop();

    /**
     * @brief 析构函数：负责 close(epfd_)
     */
    ~EventLoop();

    /**
     * @brief 开启事件分发死循环 (核心发动机)
     * * 只要 quit_ 为 false，就一直 while 循环调用 epoll_wait。
     */
    void loop();

    /**
     * @brief 更新 Channel 的 epoll 监听状态
     * * 当 Channel 想监听新事件时，会调用此函数。
     * 本函数内部会调用系统 API: epoll_ctl。
     * * @param channel 需要被更新监听状态的 Channel 对象指针
     */
    void updateChannel(Channel* channel);
};

} // namespace MyServer