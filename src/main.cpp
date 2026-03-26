/*
 * @Author: Zhang YuHua 1774630667@qq.com
 * @Date: 2026-03-17 15:35:21
 * @LastEditors: Zhang YuHua 1774630667@qq.com
 * @LastEditTime: 2026-03-26 15:03:08
 * @FilePath: /ServerPractice/src/main.cpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include "EventLoop.hpp"
#include "TcpServer.hpp"
#include "TcpConnection.hpp"
#include <iostream>
#include "ThreadPool.hpp"
#include <functional>
#include <unistd.h>
#include <signal.h>


using namespace MyServer;

int main() {
    /**
     * @brief 设置信号处理方式 (系统调用)
     * @param signum 要处理的信号 (此处为 SIGPIPE)
     * @param handler 信号处理函数或宏：
     *                - SIG_IGN: 忽略此信号。当客户端突然断开连接，而服务器仍尝试向其发送数据时，会产生 SIGPIPE 信号。
     *                           默认情况下，该信号会终止进程。设置为 SIG_IGN 可以防止服务器因此崩溃。
     *                - SIG_DFL: 使用默认处理方式。
     *                - 自定义函数指针: 调用指定的函数来处理信号。
     * @return 成功返回之前的信号处理函数指针，失败返回 SIG_ERR。
     */
    signal(SIGPIPE, SIG_IGN);
    EventLoop loop;
    TcpServer server(&loop, 8080);
    ThreadPool pool(4); // 创建一个包含4个线程的线程池

    // 核心业务逻辑：收到消息怎么办？
    server.setOnMessageCallback([&pool](const std::shared_ptr<TcpConnection>& conn, const std::string& msg) {
        // 1. 主线程收到了完整消息，直接扔给后厨 (ThreadPool)
        pool.enqueue([conn, msg]() {
            // --- 这里是工作线程 (后厨) 在运行 ---
            std::cout << "工作线程开始处理耗时任务..." << std::endl;
            sleep(3); // 模拟耗时 3 秒的数据库查询或人脸识别
            std::string result = "Processed: " + msg;
            
            // 2. 算完了！但是绝对不能直接 conn->send(result) ！！！
            // 必须打包成任务，通过 EventLoop 丢回给主线程！
            EventLoop* loop = conn->getLoop(); // 你需要在 TcpConnection 里暴露出 loop_ 指针
            
            loop->queueInLoop([conn, result]() {
                std::string http_response = 
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html; charset=utf-8\r\n"
                "Content-Length: 64\r\n"
                "\r\n"
                "<h1 style='color: red;'>Hello from My C++ Reactor Server!</h1>";

                conn->send(http_response);
            });
        });
    });

    server.start(); // 启动迎宾员
    loop.loop();    // 启动大堂经理 (死循环)

    return 0;
}