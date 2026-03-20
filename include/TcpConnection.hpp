/*
 * @Author: Zhang YuHua 1774630667@qq.com
 * @Date: 2026-03-20 15:29:42
 * @LastEditors: Zhang YuHua 1774630667@qq.com
 * @LastEditTime: 2026-03-20 15:46:19
 * @FilePath: /ServerPractice/include/TcpConnection.hpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#pragma once
#include <functional>
#include <string>

namespace MyServer {

class EventLoop;
class Channel;

/**
 * @brief TCP 连接类：代表一个已经建立连接的客户端 (专属酒席)
 * * 它的核心职责是：管理属于这个客户端的 client_fd，以及它专属的 Channel。
 * * 当这个客户端发来消息时，它负责调用 recv() 把数据读出来，
 * * 然后把读到的数据通过回调函数 (messageCallback_) 汇报给大老板 (TcpServer)。
 */
class TcpConnection {
private:
    EventLoop* loop_;       ///< 大管家 (所属的事件循环)
    int fd_;                ///< 与客户端通信的专属 fd
    Channel* channel_;      ///< 属于这个 fd 的专属通信管道 (服务员)
    std::string buffer_;    ///< 读数据的缓冲区

    // --- 给上层大老板 (TcpServer) 留的汇报接口 ---
    
    ///< 当收到客人发来的消息时，触发此回调。参数1是当前连接对象，参数2是收到的字符串
    std::function<void(TcpConnection*, const std::string&)> messageCallback_;
    
    ///< 当客人断开连接时，触发此回调。告诉大老板“客人走了，请把账本上的记录划掉”
    std::function<void(int)> closeCallback_;

public:
    TcpConnection(EventLoop* loop, int fd);
    ~TcpConnection();

    /**
     * @brief 核心方法：处理可读事件 
     * * 当底层 epoll 发现这个 client_fd 有数据来时，会通知 Channel，
     * * Channel 会执行它绑定的 readCallback_，也就是调用这个 handleRead()。
     * * 【注意】：这是你真正写 recv() 读数据的地方！
     */
    void handleRead(); 

    // --- 注册回调的 Setter ---
    void setMessageCallback(std::function<void(TcpConnection*, const std::string&)> cb) {
        messageCallback_ = std::move(cb);
    }
    void setCloseCallback(std::function<void(int)> cb) {
        closeCallback_ = std::move(cb);
    }

    /**
     * @brief 暴露给上层业务的发送数据接口
     * * 业务层调用 conn->send("Hello")，底层就在这里调用系统 send()
     */
    void send(const std::string& msg);

    int getFd() const { return fd_; }
};

} // namespace MyServer