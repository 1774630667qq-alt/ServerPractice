/*
 * @Author: Zhang YuHua 1774630667@qq.com
 * @Date: 2026-03-17 15:35:21
 * @LastEditors: Zhang YuHua 1774630667@qq.com
 * @LastEditTime: 2026-04-03 20:48:34
 * @FilePath: /ServerPractice/src/main.cpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include "EventLoop.hpp"
#include "SqlConnPool.hpp"
#include "TcpServer.hpp"
#include "TcpConnection.hpp"
#include "ThreadPool.hpp"
#include <unistd.h>
#include <signal.h>
#include "HttpResponse.hpp"
#include "HttpServer.hpp"
#include "HttpRequest.hpp"
#include "Logger.hpp"
#include <thread> // 引入并发线程库
#include "SqlConnRAII.hpp"

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
    MyServer::initGlobalLogger("MyServerLog"); // 初始化全局日志系统，日志文件前缀为 "MyServerLog"
    EventLoop loop;

    // 自动获取 CPU 核心数，并进行极其规范的性能调优级别的线程分配
    int core_num = std::thread::hardware_concurrency();
    if (core_num <= 0) {
        LOG_WARNING << "无法获取 CPU 核心数，使用默认值 4";
        core_num = 4;
    }

    // 规范的划分策略 (适用于半 IO 密集 半 计算密集的 Web 层架构)：
    // 1 个主 Reactor 线程（即当前的 main 线程，专注于 accept 接收新连接分发）
    // 剩余的一半分给 IO 子 Reactor（负责处理已建连请求的高并发读写、拆包）
    // 剩余的另一半分给 Worker 线程池（负责跑最终的业务层 Lambda 回调，防止业务逻辑耗时卡主 IO 吞吐）
    int io_threads = core_num / 2;
    int work_threads = core_num - io_threads - 1; 

    // 安全兜底保障，至少保证各有一个线程能干活
    if (io_threads <= 0) io_threads = 1;
    if (work_threads <= 0) work_threads = 1;

    LOG_INFO << "系统 CPU 核心数探测为: " << core_num << "核";

    ThreadPool pool(work_threads);
    
    // 1. 创建你的 Web 服务器
    HttpServer http_server(&loop, 8080, &pool);
    http_server.setThreadNum(io_threads);
    MyServer::SqlConnPool::Instance().Init("127.0.0.1", 3306, "root", "123456", "testdb", 10);

    // 2. 业务层专心写路由逻辑，再也不用管 TCP、线程和 fd 了！
    http_server.setHttpCallback([](const HttpRequest& req, HttpResponse& res) {
        LOG_INFO << "收到请求，路径是: " << req.getPath();

        if (req.getPath() == "/") {
            res.setStatusCode(200, "OK");
            res.addHeader("Content-Type", "text/html; charset=utf-8");
            res.setBody("<h1>欢迎来到主页！</h1><p>你的 User-Agent 是: " + req.getHeader("User-Agent") + "</p>");
        } 
        else if (req.getPath() == "/login") {
            if (req.getMethod() == "POST") {
                // 1. 从前端发来的请求体中解析出账号和密码
                // (注意：你需要自己去实现提取逻辑，假设提取为 username 和 password 变量)
                std::string username = req.getuserbybody();
                std::string password = req.getpwdbybody();

                // 2. 召唤 RAII 神器，从池子里“借”一个数据库连接
                MYSQL* sql;
                SqlConnRAII guard(&sql, &SqlConnPool::Instance());

                // 3. 拼装 SQL 语句 (注意防范 SQL 注入，但作为第一版我们可以先简单拼接)
                char sql_stmt[256] = {0};
                snprintf(sql_stmt, 256, "SELECT password FROM user WHERE username='%s'", username.c_str());

                // 4. 发射 SQL 语句！
                if (mysql_query(sql, sql_stmt)) {
                    res.setStatusCode(500, "Internal Server Error");
                    res.setBody("数据库查询崩溃啦！");
                    return; 
                }

                // 5. 提取并比对结果
                MYSQL_RES* result = mysql_store_result(sql);
                if (result != nullptr) {
                    MYSQL_ROW row = mysql_fetch_row(result);
                    res.addHeader("Content-Type", "text/html; charset=utf-8");
                    if (row != nullptr) {
                        std::string db_password(row[0]);
                        if (password == db_password) {
                            res.setStatusCode(200, "OK");
                            res.setBody("<h1>尊贵的 " + username + "，欢迎回来！</h1>");
                        } else {
                            res.setStatusCode(401, "Unauthorized");
                            res.setBody("<h1>密码错误，你是不是冒充的？</h1>");
                        }
                    } else {
                        res.setStatusCode(404, "Not Found");
                        res.setBody("<h1>用户不存在，请先注册！</h1>");
                    }
                    mysql_free_result(result); 
                }
            } else {
                res.setStatusCode(200, "OK");
                res.addHeader("Content-Type", "text/html; charset=utf-8");
                res.setBody("<form method='POST' action='/login'>账号: <input name='user'><br>密码: <input name='pwd'><br><button>登录</button></form>");
            }
        } 
        else if (req.getPath() == "/picture") {
            if (res.setFile("/home/bazinga/ServerPractice/Picture/test.jpg")) {
                res.addHeader("Content-Type", "image/jpeg");
            } else {
                res.setStatusCode(404, "Not Found");
                res.addHeader("Content-Type", "text/html; charset=utf-8");
                res.setBody("<h1 style='color:red;'>404 找不到图片</h1>");
            }
        } else if (req.getPath() == "/index") {
            if (res.setFile("/home/bazinga/ServerPractice/Html/index.html")) {
                res.addHeader("Content-Type", "text/html; charset=utf-8");
            } else {
                res.setStatusCode(404, "Not Found");
                res.addHeader("Content-Type", "text/html; charset=utf-8");
                res.setBody("<h1 style='color:red;'>404 找不到页面</h1>");
            }
        } else {
            res.setStatusCode(404, "Not Found");
            res.addHeader("Content-Type", "text/html; charset=utf-8");
            res.setBody("<h1 style='color:red;'>404 找不到页面</h1>");
        }
    });

    LOG_INFO << "Web 框架启动，监听 8080...";
    http_server.start();
    loop.loop();
    return 0;
}