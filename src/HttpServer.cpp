/*
 * @Author: Zhang YuHua 1774630667@qq.com
 * @Date: 2026-03-26 17:41:20
 * @LastEditors: Zhang YuHua 1774630667@qq.com
 * @LastEditTime: 2026-03-26 19:48:12
 * @FilePath: /ServerPractice/src/HttpServer.cpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include "HttpServer.hpp"
#include "HttpParser.hpp"
#include "TcpConnection.hpp"
#include "EventLoop.hpp"

namespace MyServer {
    HttpServer::HttpServer(EventLoop* loop, int port, ThreadPool* pool)
        : server_(loop, port), pool_(pool) {
        // 将 onMessage 绑定为 httpCallback_，让 TcpServer 在收到消息时调用 HttpServer::onMessage
        server_.setOnMessageCallback([this](std::shared_ptr<TcpConnection> conn, const std::string& msg) {
            this->onMessage(conn, msg);
        });
    }

    void HttpServer::onMessage(std::shared_ptr<TcpConnection> conn, const std::string& msg) {
        auto cb = httpCallback_; // 先把 httpCallback_ 拷贝到局部变量，避免后续被修改导致竞态条件
        // 1. 将 onMessage 的处理逻辑扔进线程池，让工作线程来执行
        pool_->enqueue([cb, conn, msg] {
            // 2. 在工作线程里，创建一个 HttpRequest 对象，调用 HttpParser::parse(msg, &request) 解析。
            HttpRequest request;
            HttpResponse response;

            // 3. 解析请求
            if (!HttpParser::parse(msg, &request)) {
                // 解析失败，准备 400 Bad Request 响应
                response.setStatusCode(400, "Bad Request");
                response.addHeader("Content-Type", "text/html; charset=utf-8");
                response.setBody("<html><head><title>400 Bad Request</title></head><body><h1>400 Bad Request</h1><p>Your browser sent a request that this server could not understand.</p></body></html>");
            } else {
                // 4. 解析成功，呼叫业务层的代码：httpCallback_(request, response); （让业务层去填 response 的内容）。
                if (cb) {
                    cb(request, response);
                }
            }

            // 5. 将最终的 response 对象序列化成字符串
            std::string response_str = response.assemble();

            // 6. 获取连接所属的 IO 线程，并将发送任务抛回给它执行
            EventLoop* loop = conn->getLoop();
            loop->queueInLoop([conn, response_str] {
                conn->send(response_str);
            });
        });
    }
} // namespace MyServer