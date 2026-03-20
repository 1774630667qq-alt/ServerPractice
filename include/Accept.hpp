/*
 * @Author: Zhang YuHua 1774630667@qq.com
 * @Date: 2026-03-19 16:38:40
 * @LastEditors: Zhang YuHua 1774630667@qq.com
 * @LastEditTime: 2026-03-19 16:38:43
 * @FilePath: /ServerPractice/include/Accept.hpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#pragma once
#include <functional>

namespace MyServer {

class EventLoop;
class Channel;

/**
 * @brief 迎宾员类：专门负责处理新客户端的连接请求
 */
class Acceptor {
private:
    EventLoop* loop_;       ///< 大管家
    int listen_fd_;         ///< 监听套接字 (大门)
    Channel* acceptChannel_;///< 专门为大门分配的通信管道
    int port_;              ///< 监听的端口号

    // 核心回调：当迎宾员成功接到新客人后，他不知道怎么安排客人，
    // 他必须通过这个回调函数，把新客人的 fd 交给餐厅大老板 (TcpServer) 去处理。
    std::function<void(int)> newConnectionCallback_; 

public:
    Acceptor(EventLoop* loop, int port);
    ~Acceptor();

    /**
     * @brief 供上层大老板调用的回调注册接口
     */
    void setNewConnectionCallback(std::function<void(int)> cb) {
        newConnectionCallback_ = std::move(cb);
    }

    /**
     * @brief 底层处理可读事件的逻辑 (被 acceptChannel_ 触发)
     */
    void handleRead();

    /**
     * @brief 开始工作：把 acceptChannel_ 挂载到 EventLoop 上
     */
    void listen();
};

} // namespace MyServer