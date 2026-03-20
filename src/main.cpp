/*
 * @Author: Zhang YuHua 1774630667@qq.com
 * @Date: 2026-03-17 15:35:21
 * @LastEditors: Zhang YuHua 1774630667@qq.com
 * @LastEditTime: 2026-03-20 16:34:03
 * @FilePath: /ServerPractice/src/main.cpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include "EventLoop.hpp"
#include "TcpServer.hpp"
#include "TcpConnection.hpp"
#include <iostream>

using namespace MyServer;

int main() {
    EventLoop loop;
    TcpServer server(&loop, 8080);

    // 核心业务逻辑：收到消息怎么办？
    server.setOnMessageCallback([](TcpConnection* conn, const std::string& msg) {
        std::cout << "业务层收到消息: " << msg << std::endl;
        // 回声逻辑：原封不动发回去
        conn->send("Echo: " + msg); 
    });

    server.start(); // 启动迎宾员
    loop.loop();    // 启动大堂经理 (死循环)

    return 0;
}