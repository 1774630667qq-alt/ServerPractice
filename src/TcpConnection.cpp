/*
 * @Author: Zhang YuHua 1774630667@qq.com
 * @Date: 2026-03-20 15:29:51
 * @LastEditors: Zhang YuHua 1774630667@qq.com
 * @LastEditTime: 2026-03-22 20:59:53
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
        int sent_bytes = 0;
        int ret = 0;
        while (true) {
            ret = ::send(fd_, msg.c_str() + sent_bytes, msg.size() - sent_bytes, 0);
            if (ret == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // 发送缓冲区满了，暂时无法继续发送
                    std::cerr << "发送缓冲区满了，消息未完全发送!" << std::endl;
                    break;
                } else {
                    std::cerr << "Send 失败!" << std::endl;
                    break;
                }
            } else {
                sent_bytes += ret;
                if (sent_bytes >= static_cast<int>(msg.size())) {
                    // 消息已经完全发送了
                    break;  
                }
            }
        }
    }
}