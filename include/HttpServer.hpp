/*
 * @Author: Zhang YuHua 1774630667@qq.com
 * @Date: 2026-03-26 17:07:48
 * @LastEditors: Zhang YuHua 1774630667@qq.com
 * @LastEditTime: 2026-03-26 17:38:23
 * @FilePath: /ServerPractice/include/HttpServer.hpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#pragma once
#include "TcpServer.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "ThreadPool.hpp"
#include <functional>
#include <memory>

namespace MyServer {

/**
 * @brief 现代 Web 服务器框架类
 * @details 封装底层 TCP 细节。业务层只需要注册 HttpCallback，处理 Request 并填充 Response。
 */
class HttpServer {
public:
    // 现代 Web 框架标志性的回调函数签名：给我一个请求，我还你一个响应
    using HttpCallback = std::function<void(const HttpRequest&, HttpResponse&)>;

private:
    TcpServer server_;          ///< 底层的 TCP 服务器
    ThreadPool* pool_;          ///< 后厨线程池 (通过指针或引用传进来)
    HttpCallback httpCallback_; ///< 业务层注册的路由回调函数

    /**
     * @brief 底层 TCP 收到消息时的中转站
     * @details 这里就是串联你所有知识点的地方！
     * * @note 【你的实现思路】
     * 1. 它是 TcpServer 的 onMessageCallback。
     * 2. 它会被扔进 pool_->enqueue() 里面执行（进入工作线程）。
     * 3. 在工作线程里，创建一个 HttpRequest 对象，调用 HttpParser::parse(msg, &request) 解析。
     * 4. 解析如果失败（比如返回 false），直接返回 400 Bad Request 给客户端。
     * 5. 如果成功，创建一个 HttpResponse response; 对象。
     * 6. ⭐ 呼叫业务层的代码：httpCallback_(request, response); （让业务层去填 response 的内容）。
     * 7. 业务层填完后，调用 response.assemble() 序列化成纯字符串。
     * 8. 最后，通过 loop->queueInLoop(...) 安全地调用 conn->send() 发送给客户端！
     */
    void onMessage(std::shared_ptr<TcpConnection> conn, const std::string& msg);

public:
    HttpServer(EventLoop* loop, int port, ThreadPool* pool);
    ~HttpServer() = default;

    /**
     * @brief 供大老板（业务层）注册路由回调
     */
    void setHttpCallback(const HttpCallback& cb) {
        httpCallback_ = cb;
    }

    /**
     * @brief 启动服务器
     */
    void start() {
        server_.start();
    }
};

} // namespace MyServer