/*
 * @Author: Zhang YuHua 1774630667@qq.com
 * @Date: 2026-03-20 16:06:48
 * @LastEditors: Zhang YuHua 1774630667@qq.com
 * @LastEditTime: 2026-03-20 16:06:54
 * @FilePath: /ServerPractice/include/TcpServer.hpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#pragma once
#include <unordered_map>
#include <functional>
#include <string>

namespace MyServer {

class EventLoop;
class Acceptor;
class TcpConnection;

/**
 * @brief 服务器主类：向开发者暴露的最外层接口 (大老板)
 * * 它负责把底层所有的零件 (EventLoop, Acceptor, TcpConnection) 组装起来。
 * * 开发者只需要实例化这个类，并设置好“收到消息怎么处理”的回调即可。
 */
class TcpServer {
private:
    EventLoop* loop_;       ///< 大管家 (外部传进来的)
    Acceptor* acceptor_;    ///< 迎宾员 (专门接收新连接)
    
    ///< 账本：记录当前餐厅里所有的客人。Key 是 fd，Value 是对应的 TcpConnection 对象
    std::unordered_map<int, TcpConnection*> connections_; 

    ///< 业务逻辑回调：让外部开发者决定，收到消息后到底该干嘛？(比如做回声、做HTTP解析等)
    std::function<void(TcpConnection*, const std::string&)> onMessageCallback_;

public:
    TcpServer(EventLoop* loop, int port);
    ~TcpServer();

    /**
     * @brief 启动服务器
     */
    void start();

    /**
     * @brief 迎宾员接到新客人后，触发此函数
     * @param fd 新客人的文件描述符
     */
    void newConnection(int fd);

    /**
     * @brief 客人离开时，触发此函数 (主要为了从账本 connections_ 中删掉记录)
     * @param fd 离开的客人的文件描述符
     */
    void removeConnection(int fd);

    // --- 给外部开发者的接口 ---
    void setOnMessageCallback(std::function<void(TcpConnection*, const std::string&)> cb) {
        onMessageCallback_ = std::move(cb);
    }
};

} // namespace MyServer