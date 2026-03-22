/*
 * @Author: Zhang YuHua 1774630667@qq.com
 * @Date: 2026-03-20 15:29:51
 * @LastEditors: Zhang YuHua 1774630667@qq.com
 * @LastEditTime: 2026-03-22 22:38:48
 * @FilePath: /ServerPractice/src/TcpConnection.cpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include <iostream>
#include "TcpConnection.hpp"
#include "EventLoop.hpp"
#include "Channel.hpp"
#include "Buffer.hpp"
#include <sys/socket.h>
#include <unistd.h>

namespace MyServer {
    TcpConnection::TcpConnection(EventLoop* loop, int fd)
        : loop_(loop), fd_(fd) {
        channel_ = new Channel(loop_, fd_);
        // 绑定读事件的回调函数
        channel_->setReadCallback(std::bind(&TcpConnection::handleRead, this));
        channel_->setWriteCallback(std::bind(&TcpConnection::handleWrite, this));
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
                buffer_.append(buf, bytes_read); // 把读到的数据追加到缓冲区
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

        // 读到数据了，进行解包
        while (true) {
            std::string msg = buffer_.extractMessage();
            if (msg.empty()) {
                // 没有完整消息了，退出解包循环
                break; 
            }
            // 触发业务层回调，告诉大老板收到消息了
            if (messageCallback_) {
                messageCallback_(this, msg);
            }
        }
    }

    void TcpConnection::send(const std::string& msg) {
        // 1. 检查是否有未发送完的数据在缓冲区里，如果有，先把它们发完
        if (writeBuffer_.size() > 0) {
            while (true) {
                std::string msg = writeBuffer_.extractMessage();
                if (msg.empty()) {
                    // 没有完整消息了，退出解包循环
                    break;
                }
                ::send(fd_, msg.c_str(), msg.size(), 0);
            }
        }

        // 2. 直接发送消息
        ssize_t bytes_sent = ::send(fd_, msg.c_str(), msg.size(), 0);
        std::cout << "实际发送字节数: " << bytes_sent << std::endl;

        // 3. 检查是否全部发送成功了
        if (bytes_sent == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 发送缓冲区满了，暂时无法发送，应该把剩余数据放到 writeBuffer_ 中，等下次可写事件再发
                writeBuffer_.append(msg.c_str(), msg.size());
                channel_->enableWriting(); // 启用写事件，等下次可写时再发
            } else {
                std::cerr << "Send 失败!" << std::endl;
                // 同理，拷贝到栈上安全执行
                auto cb = closeCallback_;
                if (cb) {
                    cb(fd_);
                }
            }
        } else if (static_cast<size_t>(bytes_sent) < msg.size()) {
            // 只发出了一部分数据，也需要把剩余数据放到 writeBuffer_ 中
            writeBuffer_.append(msg.c_str() + bytes_sent, msg.size() - bytes_sent);
            channel_->enableWriting(); // 启用写事件，等下次可写时再发
        } 
    }

    void TcpConnection::handleWrite() {
        if (writeBuffer_.size() > 0) {
            while (true) {
                std::string msg = writeBuffer_.extractMessage();
                if (msg.empty()) {
                    // 没有完整消息了，退出解包循环
                    break;
                }
                // 触发业务层回调，告诉大老板收到消息了
                if (messageCallback_) {
                    messageCallback_(this, msg);
                }
            }
        } 
        if (writeBuffer_.size() == 0) {
            // 已经把所有数据发完了，关闭写事件
            channel_->disableWriting();
        }
    }
}