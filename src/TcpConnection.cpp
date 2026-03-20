#include <iostream>
#include "TcpConnection.hpp"
#include "EventLoop.hpp"
#include "Channel.hpp"
#include <sys/socket.h>
#include <unistd.h>

namespace MyServer {
    TcpConnection::TcpConnection(EventLoop* loop, int fd)
        : loop_(loop), fd_(fd) {
        channel_ = new Channel(loop_, fd_);
        // 绑定读事件的回调函数
        channel_->setReadCallback(std::bind(&TcpConnection::handleRead, this));
        channel_->enableReading(); // 启用读事件
    }

    TcpConnection::~TcpConnection() {
        delete channel_;
        ::close(fd_); // 必须关闭底层文件描述符，防止幽灵连接
    }

    void TcpConnection::handleRead() {
        int active_fd = channel_->getFd();
        while (true) {
            char buf[1024];
            int bytes_read = recv(active_fd, buf, sizeof(buf), 0);
            if (bytes_read > 0) {
                if (messageCallback_) {
                    messageCallback_(this, std::string(buf, bytes_read));
                }
            } else if (bytes_read == -1){
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // 没有更多数据可读了，退出循环
                    break;
                } else {
                    std::cerr << "Recv 失败!" << std::endl;
                    // 将回调拷贝到栈上，防止对象自杀导致段错误
                    auto cb = closeCallback_;
                    if (cb) {
                        cb(fd_);
                    }
                    break;
                }
            } else {
                // bytes_read == 0，客户端断开连接
                std::cout << "客户端 fd " << active_fd << " 断开连接" << std::endl;
                // 同理，拷贝到栈上安全执行
                auto cb = closeCallback_;
                if (cb) {
                    cb(fd_);
                }
                break;
            }
        }
    }

    void TcpConnection::send(const std::string& msg) {
        ::send(fd_, msg.c_str(), msg.size(), 0);
    }
}