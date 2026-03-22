/*
 * @Author: Zhang YuHua 1774630667@qq.com
 * @Date: 2026-03-20 15:29:42
 * @LastEditors: Zhang YuHua 1774630667@qq.com
 * @LastEditTime: 2026-03-22 22:15:35
 * @FilePath: /ServerPractice/include/TcpConnection.hpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#pragma once
#include <functional>
#include <string>
#include "Buffer.hpp"

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
    Buffer buffer_;         ///< 读数据的缓冲区
    Buffer writeBuffer_;    ///< 写数据的缓冲区

    // --- 给上层大老板 (TcpServer) 留的汇报接口 ---
    
    ///< 当收到客人发来的消息时，触发此回调。参数1是当前连接对象，参数2是收到的字符串
    std::function<void(TcpConnection*, const std::string&)> messageCallback_;
    
    ///< 当客人断开连接时，触发此回调。告诉大老板“客人走了，请把账本上的记录划掉”
    std::function<void(int)> closeCallback_;

public:
    /**
     * @brief 构造函数：接管客户端文件描述符，并初始化其专属 Channel
     * @param loop 所在的 EventLoop 实例
     * @param fd 客户端已连接的非阻塞套接字文件描述符
     */
    TcpConnection(EventLoop* loop, int fd);
    
    /**
     * @brief 析构函数：释放内部专属 Channel，并严格调用 close(fd_) 关闭底层 TCP 通信套接字
     */
    ~TcpConnection();

    /**
     * @brief 核心方法：处理套接字可读事件
     * @details 因为使用的是 EPOLLET (边缘触发) 模式，所以在该函数内部，
     * 会在一个死循环中不断调用 recv() 读取数据，直到返回 EAGAIN 或 EWOULDBLOCK 为止。
     * - 读到数据时，将触发 messageCallback_ 向上层抛出。
     * - 遇到 0 字节断开或不可恢复的错误时，将触发 closeCallback_ 并安全地向上层通知注销自己。
     */
    void handleRead(); 

    /**
     * 
     */
    void handleWrite();

    // --- 注册回调的 Setter ---
    void setMessageCallback(std::function<void(TcpConnection*, const std::string&)> cb) {
        messageCallback_ = std::move(cb);
    }
    void setCloseCallback(std::function<void(int)> cb) {
        closeCallback_ = std::move(cb);
    }

    /**
     * @brief 同步发送数据到客户端套接字
     * @param msg 要发送的字符串数据
     * @note 当前版本为直接调用底层系统 send()。如果缓冲区满，可能会导致发送不完整。后续可扩展应用层发送缓冲区。
     */
    void send(const std::string& msg);

    int getFd() const { return fd_; }
};

} // namespace MyServer