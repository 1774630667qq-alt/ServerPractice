/*
 * @Author: Zhang YuHua 1774630667@qq.com
 * @Date: 2026-03-17 16:27:59
 * @LastEditors: Zhang YuHua 1774630667@qq.com
 * @LastEditTime: 2026-03-21 20:26:17
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
#include <cerrno>       // 提供 errno 报错解析
#include "ThreadPool.hpp" // 提供线程池类定义
#include "sys/epoll.h"    // 提供 epoll 函数及数据结构
#include <fcntl.h>      // 提供 fcntl

// 辅助函数：将文件描述符设置为非阻塞模式
void setNonBlocking(int fd) {
    /**
     * @brief 操作文件描述符，控制其属性和行为
        * @signature int fcntl(int fd, int cmd, ...);
        * @param fd 要操作的文件描述符
        * @param cmd 对文件描述符执行的操作命令。
        *            - F_GETFL: 获取文件访问模式(如 O_RDONLY, O_RDWR)和文件状态标志
        *            - F_SETFL: 设置文件状态标志。常见的文件状态标志包括：
        *                       * O_NONBLOCK: 非阻塞模式，读写不阻塞，
        *                          无数据时立即返回 EAGAIN/EWOULDBLOCK（最常配合 epoll 使用）
        *                       * O_APPEND: 追加模式，每次写操作都追加到文件末尾
        *                       * O_ASYNC: 信号驱动 I/O，文件可读写时内核发送 SIGIO 信号
        *                       * O_SYNC: 同步写入，强制等待数据物理写入底层硬件
        *                       （注：F_SETFL 不能修改文件访问模式和文件创建标志，如 O_CREAT）
        * @param ... (可选参数) 依据 cmd 的不同而不同。当使用 F_SETFL 时，传入要设置的新标志
        * @return 成功时的返回值取决于 cmd (F_GETFL 返回当前标志位，F_SETFL 返回 0)，失败返回 -1
        */
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

    // 开启端口复用
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(listen_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        std::cerr << "Bind 失败! 错误原因: " << strerror(errno) << std::endl;
        return -1;
    }
    std::cout << "Bind 绑定成功!" << std::endl;

    // 监听端口
    if (listen(listen_fd, 5) == -1) {
        std::cerr << "Listen 失败!" << std::endl;
        close(listen_fd);
        return -1;
    }
    setNonBlocking(listen_fd); // 将监听套接字设置为非阻塞模式
    std::cout << "Listen 成功，服务器正在监听 8080 端口..." << std::endl;
    
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
    std::cout << "Epoll 初始化成功，开始等待客户端连接..." << std::endl;

    const int MAX_EVENTS = 1024;
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
                // 有新客户端连接
                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                int client_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &client_len);
                if (client_fd == -1) {
                    std::cerr << "Accept 失败!" << std::endl;
                    continue;
                }

                struct epoll_event client_ev;
                client_ev.data.fd = client_fd;
                client_ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT; // 关注可读事件，并开启边缘触发(ET)
                if (epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &client_ev) == -1) {
                    std::cerr << "Epoll 添加客户端套接字失败!" << std::endl;
                    close(client_fd);
                    continue;
                }

                setNonBlocking(client_fd);

                char ip_str[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, sizeof(ip_str));
                std::cout << "新客户端连接: " << ip_str << ":" << ntohs(client_addr.sin_port) << ", fd: " << client_fd << std::endl;
            } else {
                std::thread([active_fd, epfd] {
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
                }).detach();
            }
        }
    }

    close(listen_fd);
    return 0;
}