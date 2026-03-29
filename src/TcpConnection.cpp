/*
 * @Author: Zhang YuHua 1774630667@qq.com
 * @Date: 2026-03-20 15:29:51
 * @LastEditors: Zhang YuHua 1774630667@qq.com
 * @LastEditTime: 2026-03-29 22:55:43
 * @FilePath: /ServerPractice/src/TcpConnection.cpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include "TcpConnection.hpp"
#include "EventLoop.hpp"
#include "Channel.hpp"
#include "Logger.hpp"
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
        // 使用智能指针守卫，防止在回调过程中自己被析构导致崩溃
        auto guard = shared_from_this();
        int active_fd = channel_->getFd();
        while (true) {
            char buf[1024];
            /**
             * @brief 从套接字接收数据 (系统调用)
             * @param sockfd  用于接收数据的套接字文件描述符 (此处的 active_fd)
             * @param buf     指向存放接收数据缓冲区的指针
             * @param len     缓冲区的最大长度
             * @param flags   接收操作的标志位，通常设为 0
             * @return 成功返回实际接收到的字节数；返回 0 表示对端已正常关闭连接；失败返回 -1 并设置 errno
             */
            int bytes_read = recv(active_fd, buf, sizeof(buf), 0);
            if (bytes_read > 0) {
                buffer_.append(buf, bytes_read); // 把读到的数据追加到缓冲区
                // 每次读到数据都要续命一下，重置秒表
                extendLife();
            } else if (bytes_read == -1){
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // 没有更多数据可读了，退出循环
                    break;
                } else {
                    LOG_ERROR << "Recv 失败!";
                    // 将回调拷贝到栈上，防止对象自杀导致段错误
                    auto cb = closeCallback_;
                    if (cb) {
                        cb(guard);
                    }
                    break;
                }
            } else {
                // bytes_read == 0，客户端断开连接
                LOG_INFO << "客户端 fd " << active_fd << " 断开连接";
                // 同理，拷贝到栈上安全执行
                auto cb = closeCallback_;
                if (cb) {
                    cb(guard);
                }
                break;
            }
        }

        // 读到数据了，进行解包
        while (true) {
            std::string msg = buffer_.extractHttpHeaders();
            if (msg.empty()) {
                // 没有完整消息了，退出解包循环
                break; 
            }
            // 触发业务层回调，告诉大老板收到消息了
            if (messageCallback_) {
                messageCallback_(guard, msg);
            }
        }
    }

    void TcpConnection::send(const std::string& msg) {
        auto guard = shared_from_this();
        // 1. 如果写缓冲区已经有数据，说明之前就没发完，TCP 处于“堵车”状态。
        // 此时绝对不能直接去插队，必须把新消息排在写缓冲区的末尾！
        if (writeBuffer_.size() > 0) {
            writeBuffer_.append(msg.data(), msg.size());
            return;
        }

        // 2. 如果写缓冲区是空的，说明一路畅通，尝试直接发送！
        /**
         * @brief 向套接字发送数据 (系统调用)
         * @param sockfd  用于发送数据的套接字文件描述符 (此处的 fd_)
         * @param buf     指向包含要发送数据的缓冲区的指针
         * @param len     要发送数据的字节数
         * @param flags   发送操作的标志位，通常设为 0
         * @return 成功返回实际发送的字节数（在非阻塞模式下可能小于请求的长度）；失败返回 -1 并设置 errno
         */
        ssize_t bytes_sent = ::send(fd_, msg.data(), msg.size(), 0);
        
        // 3. 处理发送结果
        if (bytes_sent == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                bytes_sent = 0; // 缓冲区满了，一个字节都没发出去，正常情况
            } else {
                LOG_ERROR << "Send 失败! errno=" << errno;
                auto cb = closeCallback_;
            if (cb) cb(guard);
                return;
            }
        }

        // 4. 如果还有没发完的剩余数据，存入 writeBuffer_，并让大堂经理开始盯防写事件！
        if (static_cast<size_t>(bytes_sent) < msg.size()) {
            writeBuffer_.append(msg.data() + bytes_sent, msg.size() - bytes_sent);
            channel_->enableWriting(); // 开启 EPOLLOUT 监听
        } 
    }

    void TcpConnection::handleWrite() {
        auto guard = shared_from_this();
        // 这个函数只有在 epoll 发现底层网卡腾出空位时才会被调用
        if (writeBuffer_.size() > 0) {
            // 直接无脑把缓冲区里所有的数据推给网卡
            /**
             * @brief 向套接字发送数据 (系统调用)
             * @param sockfd  用于发送数据的套接字文件描述符 (此处的 fd_)
             * @param buf     指向写缓冲区数据的指针
             * @param len     写缓冲区当前积压数据的字节数
             * @param flags   发送操作的标志位，通常设为 0
             * @return 成功返回实际发送的字节数；失败返回 -1 并设置 errno
             */
            ssize_t bytes_sent = ::send(fd_, writeBuffer_.data(), writeBuffer_.size(), 0);
            
            if (bytes_sent > 0) {
                // 发送成功了多少，就从缓冲区里删掉多少！
                writeBuffer_.retrieve(bytes_sent); 
            } else if (bytes_sent == -1) {
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    LOG_ERROR << "handleWrite 底层发送失败!";
                    auto cb = closeCallback_;
                if (cb) cb(guard);
                    return;
                }
            }
        }

        // ⭐ 极其致命的判断：如果全部发完了，务必关闭写事件！
        if (writeBuffer_.size() == 0) {
            channel_->disableWriting();
        }
    }

    void TcpConnection::extendLife() {
        // 1. 如果之前已经有一个秒表了，我们直接把它“标记删除”（惰性删除，O(1)复杂度）
        // 这样大管家在处理时会自动忽略它，极其高效！
        if (keepAliveTimer_) {
            keepAliveTimer_->setDeleted();
        }

        // 2. 重新开启一个 30 秒的定时器！
        std::weak_ptr<TcpConnection> weak_conn = shared_from_this();

        keepAliveTimer_ = loop_->runAfter(30000, [weak_conn]() {
            // 闹钟响了，尝试把 weak_ptr 提升为 shared_ptr
            auto conn = weak_conn.lock();
            if (conn) {
                // 如果提升成功，说明连接还没被常规途径关闭，立刻执行踢人逻辑！
                conn->handleTimeout();
            }
        });
    }

    void TcpConnection::handleTimeout() {
        LOG_WARNING << "客户端 fd " << fd_ << " 长时间未发送数据，心跳超时，强制踢出！";
        // 触发关闭回调，TcpServer 会负责把它从账本里删掉，并销毁堆内存
        auto guard = shared_from_this();
        if (closeCallback_) {
            closeCallback_(guard);
        }
    }
}