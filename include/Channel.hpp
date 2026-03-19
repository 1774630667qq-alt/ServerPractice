/*
 * @Author: Zhang YuHua 1774630667@qq.com
 * @Date: 2026-03-19 15:19:59
 * @LastEditors: Zhang YuHua 1774630667@qq.com
 * @LastEditTime: 2026-03-19 15:54:55
 * @FilePath: /ServerPractice/include/Channel.hpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#pragma once
#include <functional>
#include <sys/epoll.h>

namespace MyServer {

class EventLoop; // 前向声明，避免头文件循环依赖

/**
 * @brief 通信管道类 (Reactor 模式的核心组件之一)
 * * 它的核心职责是：封装一个文件描述符 (fd) 以及这个 fd 所感兴趣的事件 (events)。
 * 它不直接与操作系统底层的 epoll 打交道，而是把自己的需求告诉 EventLoop。
 * 当事件真正发生时，EventLoop 会调用 Channel 的 handleEvent()，
 * Channel 再根据具体发生的事件，去执行对应的回调函数。
 */
class Channel {
private:
    EventLoop* loop_;       ///< 指向当前 Channel 所属的 EventLoop 大管家
    const int fd_;          ///< 封装的底层的系统文件描述符 (不可变)
    uint32_t events_;       ///< 我【希望】大管家帮我监听的事件掩码 (如 EPOLLIN, EPOLLET)
    uint32_t revents_;      ///< 大管家【告诉我】实际发生的事件掩码 (由 epoll_wait 返回)

    // 核心：使用你之前自己写的 MyStl::function，或者 std::function 都可以。
    // 这里保存的是高层业务逻辑注册进来的代码块。
    std::function<void()> readCallback_;  ///< 发生可读事件时要执行的函数
    std::function<void()> closeCallback_; ///< 发生关闭事件时要执行的函数

public:
    /**
     * @brief 构造函数
     * @param loop 所属的 EventLoop 指针
     * @param fd 需要被封装的文件描述符
     */
    Channel(EventLoop* loop, int fd);
    ~Channel() = default;

    /**
     * @brief 核心分发逻辑：根据实际发生的事件 (revents_)，调用对应的回调函数
     * * 这个函数是由 EventLoop 在 epoll_wait 醒来后主动调用的！
     */
    void handleEvent();

    /**
     * @brief 注册读事件回调函数
     * @param cb 外部传入的 Lambda 表达式或函数指针
     */
    void setReadCallback(std::function<void()> cb) { readCallback_ = std::move(cb); }

    /**
     * @brief 注册关闭事件回调函数
     */
    void setCloseCallback(std::function<void()> cb) { closeCallback_ = std::move(cb); }

    /**
     * @brief 开启对“可读事件”的监听
     * * 业务层调用这个方法后，Channel 会修改自己的 events_，
     * 并通知底层的 EventLoop 去更新 epoll 树。
     */
    void enableReading();

    // --- Getter 和 Setter ---
    int getFd() const { return fd_; }
    uint32_t getEvents() const { return events_; }
    
    /**
     * @brief 设置实际发生的事件 (由 EventLoop 在 epoll_wait 后调用)
     * @param revt epoll 返回的活跃事件掩码
     */
    void setRevents(uint32_t revt) { revents_ = revt; }
};

} // namespace MyServer