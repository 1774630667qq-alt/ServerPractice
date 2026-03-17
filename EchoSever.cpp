/*
 * @Author: Zhang YuHua 1774630667@qq.com
 * @Date: 2026-03-17 16:27:59
 * @LastEditors: Zhang YuHua 1774630667@qq.com
 * @LastEditTime: 2026-03-17 22:31:56
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
#include "ThreadPool.hpp" // 提供线程池类定义

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
        std::cerr << "Bind 失败!" << std::endl;
        return -1;
    }

    // 监听端口
    if (listen(listen_fd, 5) == -1) {
        std::cerr << "Listen 失败!" << std::endl;
        return -1;
    }
    std::cout << "服务器启动，正在监听 8080 端口..." << std::endl;
    // 创建线程池，假设我们创建一个包含 2 个工作线程的线程池
    MyServer::ThreadPool pool(2);

    // 创建客户端地址结构
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    while (true) {
        // 接受客户端连接
        int client_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd == -1) {
            std::cerr << "Accept 失败!" << std::endl;
            continue; // 等待下一个连接
        }
        // 创建线程处理客户端通信
        auto task = [client_fd] () {
            std::cout << "有新客户端连上来了！分配的通信 fd: " << client_fd << std::endl;
            char buffer[1024];
            while (true) {
                ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer), 0);
                if (bytes_read <= 0) {
                    std::cout << "客户端断开连接，fd: " << client_fd << std::endl;
                    close(client_fd);
                    break; // 退出循环，结束线程
                }
                // 回显数据
                std::cout << "收到客户端消息: " << buffer << std::endl;
                send(client_fd, buffer, bytes_read, 0);
            }
        };
        pool.enqueue(std::move(task)); // 将任务添加到线程池
    }
    close(listen_fd);
    return 0;
}