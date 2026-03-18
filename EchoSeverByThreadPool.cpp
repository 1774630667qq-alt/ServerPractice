/*
 * @Author: Zhang YuHua 1774630667@qq.com
 * @Date: 2026-03-17 16:27:59
 * @LastEditors: Zhang YuHua 1774630667@qq.com
 * @LastEditTime: 2026-03-18 20:23:59
 * @FilePath: /ServerPractice/EchoSever.cpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
/**
 * @brief 回声服务器的示例代码
 */

#include <sys/socket.h> // 提供 socket 函数及数据结构
#include <netinet/in.h> // 提供 IPv4 的 sockaddr_in 结构体
#include <arpa/inet.h>  // 提供 IP 地址转换函数
#include <unistd.h>     // 提供 close 函数
#include <iostream>
#include <cstring>      // 提供 memset
#include <thread>       // 提供 std::thread
#include "sys/epoll.h"    // 提供 epoll 函数及数据结构
#include "include/ThreadPool.hpp" // 提供线程池类定义
#include <fcntl.h>      // 提供 fcntl

void setNonBlocking(int fd) {
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
}

int main () {
    // 创建监听套接字
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == -1) {
        std::cerr << "Socket 创建失败!" << std::endl;
        return -1;
    }
    std::cout << "Socket 创建成功，fd: " << listen_fd << std::endl;

    // 绑定地址和端口
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr)); // 清零
    server_addr.sin_family = AF_INET;             // IPv4
    server_addr.sin_port = htons(8080);           // 端口号
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // 监听所有网卡
    std::cout << "初始化服务器地址结构成功!" << std::endl;

    // 开启端口复用
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(listen_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        std::cerr << "Bind 失败!" << std::endl;
        close(listen_fd);
        return -1;
    }
    std::cout << "绑定地址和端口成功!" << std::endl;

    // 监听端口
    if (listen(listen_fd, 5) == -1) {
        std::cerr << "Listen 失败!" << std::endl;
        close(listen_fd);
        return -1;
    }
    std::cout << "服务器启动，正在监听 8080 端口..." << std::endl;
    // 创建线程池，假设我们创建一个包含 2 个工作线程的线程池
    MyServer::ThreadPool pool(2);

    int epfd = epoll_create1(0);
    if (epfd == -1) {
        std::cerr << "Epoll 创建失败!" << std::endl;
        close(listen_fd);
        return -1;
    }

    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = listen_fd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &event) == -1) {
        std::cerr << "Epoll 添加监听套接字失败!" << std::endl;
        close(listen_fd);
        close(epfd);
        return -1;
    }
    
    const int MAX_EVENTS = 10;
    struct epoll_event active_events[MAX_EVENTS];

    while (true) {
        int nfds = epoll_wait(epfd, active_events, MAX_EVENTS, -1);
        if (nfds == -1) {
            std::cerr << "Epoll 等待事件失败!" << std::endl;
            break;
        }

        for (int i = 0; i < nfds; ++i) {
            int active_fd = active_events[i].data.fd;

            if (active_fd == listen_fd) {
                // 有新客户端连接进来
                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                int client_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &client_len);
                if (client_fd == -1) {
                    std::cerr << "Accept 失败!" << std::endl;
                    continue;
                }

                setNonBlocking(client_fd);

                // 将新客户端注册给 epoll
                struct epoll_event client_ev;
                client_ev.data.fd = client_fd;
                /**
                 * @brief 新增 EPOLLONESHOT 选项，用于防止“惊群效应”
                 * @details
                 * 在多线程环境下，当一个文件描述符就绪时，多个线程可能同时从 epoll_wait() 中被唤醒，
                 * 这就是所谓的“惊群”。EPOLLONESHOT 确保一个文件描述符在被触发一次后，就会自动从 epoll 实例的
                 * 监听列表中移除。这样，只有一个线程能处理该事件。
                 * @warning
                 * 处理完该文件描述符的 I/O 后，必须手动调用 epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev)
                 * 重新注册该文件描述符，否则它将不再被监听，导致后续事件丢失。
                 * (注：当前代码示例中缺少了重新注册的步骤，这是一个潜在的 BUG)
                 */
                client_ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
                if (epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &client_ev) == -1) {
                    std::cerr << "Epoll 添加客户端套接字失败!" << std::endl;
                    close(client_fd);
                } else {
                    char ip_str[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, sizeof(ip_str));
                    std::cout << "新客户端连接: " << ip_str << ":" << ntohs(client_addr.sin_port) << ", fd: " << client_fd << std::endl;
                }
            } else {
                // 某个已连接的客户端有数据可读了，我们把这个任务交给线程池去处理
                pool.enqueue([active_fd, epfd] {
                    char buffer[1024];
                    while (true) {
                        ssize_t bytes_read = recv(active_fd, buffer, sizeof(buffer), 0);
                        if (bytes_read > 0) {
                            buffer[bytes_read] = '\0';
                            std::cout << "收到来自 fd " << active_fd << " 的消息: " << buffer << std::endl;
                            send(active_fd, buffer, bytes_read, 0);
                        } else if (bytes_read == -1) {
                            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                                // 没有更多数据可读了，回复修改
                                struct epoll_event ev;
                                ev.data.fd = active_fd;
                                ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
                                epoll_ctl(epfd, EPOLL_CTL_MOD, active_fd, &ev);
                                break;
                            } else {
                                std::cerr << "接收数据失败!" << std::endl;
                                close(active_fd);
                                break;
                            }
                        } else {
                            // bytes_read == 0，客户端断开连接
                            std::cout << "客户端 fd " << active_fd << " 断开连接" << std::endl;
                            close(active_fd);
                            break;
                        }   
                    }
                });
            }
        }
    }
    
    close(listen_fd);
    return 0;
}