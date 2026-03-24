/*
 * @Author: Zhang YuHua 1774630667@qq.com
 * @Date: 2026-03-17 15:35:21
 * @LastEditors: Zhang YuHua 1774630667@qq.com
 * @LastEditTime: 2026-03-24 22:40:49
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


using namespace MyServer;

int main() {
    EventLoop loop;
    TcpServer server(&loop, 8080);
    ThreadPool pool(4); // 创建一个包含4个线程的线程池

    // 核心业务逻辑：收到消息怎么办？
    server.setOnMessageCallback([&pool](TcpConnection* conn, const std::string& msg) {
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
                // --- 这里又回到了主线程在运行！---
                // 现在在主线程调用 send，绝对线程安全！
                conn->send(result); 
            });
        });
    });

    server.start(); // 启动迎宾员
    loop.loop();    // 启动大堂经理 (死循环)

    return 0;
}