/*
 * @Author: Zhang YuHua 1774630667@qq.com
 * @Date: 2026-03-20 16:06:42
 * @LastEditors: Zhang YuHua 1774630667@qq.com
 * @LastEditTime: 2026-03-29 22:57:01
 * @FilePath: /ServerPractice/src/TcpServer.cpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include "TcpServer.hpp"
#include "EventLoop.hpp"
#include "Accept.hpp"
#include "Logger.hpp"
#include "TcpConnection.hpp"

namespace MyServer {

TcpServer::TcpServer(EventLoop* loop, int port)
    : loop_(loop), acceptor_(nullptr) {
    acceptor_ = new Acceptor(loop_, port);
    
    // 告诉迎宾员：接到新客人后，把 fd 交给我的 newConnection 方法！
    acceptor_->setNewConnectionCallback(
        std::bind(&TcpServer::newConnection, this, std::placeholders::_1)
    );
}

TcpServer::~TcpServer() {
    delete acceptor_;
    // 清理所有尚未关闭的连接
    connections_.clear();
}

void TcpServer::start() {
    acceptor_->listen();
}

void TcpServer::newConnection(int fd) {
    // 1. 创建一个新的 TcpConnection 对象
    std::shared_ptr<TcpConnection> conn = std::make_shared<TcpConnection>(loop_, fd);
    
    // 2. 告诉客人：如果你收到消息，请立刻执行我的 onMessageCallback_！
    conn->setMessageCallback(onMessageCallback_);
    
    // 3. 告诉客人：如果你走了，请调用我的 removeConnection 方法告诉我！
    conn->setCloseCallback(
        std::bind(&TcpServer::removeConnection, this, std::placeholders::_1)
    );

    // 开启连接的心跳监测
    conn->extendLife();
    
    // 4. 把这个新客人登记到账本上
    connections_[fd] = conn;
    LOG_INFO << "TcpServer: 新连接加入账本，当前连接数=" << connections_.size();
}

void TcpServer::removeConnection(const std::shared_ptr<TcpConnection>& conn) {
    // 【任务 4】：客人走了，划掉账本
    if (connections_.find(conn->getFd()) != connections_.end()) {
        connections_.erase(conn->getFd()); // 从账本中移除
        // 对象会自动引用计数减一，如果是最后一个引用，则会自动销毁内存和 close(fd)
        LOG_INFO << "TcpServer: 连接已从账本移除，当前连接数=" << connections_.size();
    }
}

} // namespace MyServer