#include <iostream>
#include "TcpConnection.hpp"
#include "EventLoop.hpp"
#include "Channel.hpp"
#include <sys/socket.h>

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
    }

    void TcpConnection::handleRead() {
        int active_fd = channel_->getFd();
        while (true) {
            std::cout << "up to this" << std::endl;
            int bytes_read = recv(active_fd, buffer_.data(), buffer_.size(), 0);
            std::cout << "bytes_read: " << bytes_read << std::endl;
            if (bytes_read > 0) {
                buffer_[bytes_read] = '\0'; // 确保字符串结尾
                if (messageCallback_) {
                    messageCallback_(this, std::string(buffer_));
                }
            } else if (bytes_read == -1){
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // 没有更多数据可读了，退出循环
                    break;
                } else {
                    std::cerr << "Recv 失败!" << std::endl;
                    if (closeCallback_) {
                        closeCallback_(fd_);
                    }
                    break;
                }
            } else {
                // bytes_read == 0，客户端断开连接
                std::cout << "客户端 fd " << active_fd << " 断开连接" << std::endl;
                if (closeCallback_) {
                    closeCallback_(fd_);
                }
                break;
            }
        }
    }

    void TcpConnection::send(const std::string& msg) {
        ::send(fd_, msg.c_str(), msg.size(), 0);
    }
}