/*
 * @Author: Zhang YuHua 1774630667@qq.com
 * @Date: 2026-03-17 15:35:21
 * @LastEditors: Zhang YuHua 1774630667@qq.com
 * @LastEditTime: 2026-04-04 15:09:59
 * @FilePath: /ServerPractice/src/main.cpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include "EventLoop.hpp"
#include "SqlConnPool.hpp"
#include "TcpServer.hpp"
#include "TcpConnection.hpp"
#include "ThreadPool.hpp"
#include <read.h>
#include <unistd.h>
#include <signal.h>
#include "HttpResponse.hpp"
#include "HttpServer.hpp"
#include "HttpRequest.hpp"
#include "Logger.hpp"
#include <thread> // 引入并发线程库
#include "SqlConnRAII.hpp"
#include <random>
#include <hiredis/hiredis.h>
#include "RedisConnRAII.hpp"
#include "RedisConnPool.hpp"

/**
 * @brief 生成密码学安全的随机字符串，用作登录凭证
 * 
 * @param length 随机字符串的长度
 * @return std::string 生成的随机字符串
 */
std::string generateSecureCookieToken(size_t length) {
    // 定义字符集
    std::string base = 
                "abcdefghijklmnopqrstuvwxyz"
                "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                "0123456789"
                "!@#$%^&*()";
    
    std::string token;
    token.reserve(length);
    
    // 提供密码学安全的随机数发生器
    std::random_device rd;

    // 使用均匀分布，避免取模偏差
    std::uniform_int_distribution<size_t> dist(0, base.size() - 1);

    for (size_t i = 0; i < length; ++i) {
        token += base[dist(rd)];
    }
    return token;
}

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
    MyServer::RedisConnPool::Instance().Init("127.0.0.1", 6379, "123456", 10);

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

                // 3. 拼装 SQL 语句
                char sql_stmt[256] = {0};
                snprintf(sql_stmt, 256, "SELECT password FROM user WHERE username='%s'", username.c_str());

                // 4. 发射 SQL 语句！
                /**
                 * @brief 执行指向由null结尾的字符串的SQL查询
                 * @signature int mysql_query(MYSQL *mysql, const char *stmt_str);
                 * @param mysql 连接句柄
                 * @param stmt_str 以null结尾的包含要执行SQL语句的字符串
                 * @return 成功返回0，如果有错误发生则返回非0值
                 */
                if (mysql_query(sql, sql_stmt)) {
                    res.setStatusCode(500, "Internal Server Error");
                    res.setBody("数据库查询崩溃啦！");
                    return; 
                }

                // 5. 提取并比对结果
                /**
                 * @brief MYSQL_RES是一个结构体，用于表示返回行的查询结果(如SELECT, SHOW, DESCRIBE, EXPLAIN)
                 * @brief mysql_store_result()将查询的全部结果读取到客户端，分配一个MYSQL_RES结构体，并将结果放入此结构中
                 * @signature MYSQL_RES *mysql_store_result(MYSQL *mysql);
                 * @param mysql 连接句柄
                 * @return 成功返回包含查询结果的MYSQL_RES结果集指针，如果出错或没有结果(例如是一个INSERT语句)则返回nullptr
                 */
                MYSQL_RES* result = mysql_store_result(sql);
                if (result != nullptr) {
                    /**
                     * @brief MYSQL_ROW是字符串数组的安全表示形式。它指向结果集中的当前行，列值可以当作字符串访问
                     * @brief mysql_fetch_row()从结果集中检索下一行
                     * @signature MYSQL_ROW mysql_fetch_row(MYSQL_RES *result);
                     * @param result 通过mysql_store_result()获得的结果集指针
                     * @return 成功返回包含下一行值的MYSQL_ROW结构，如果没有更多的行可供检索或发生错误，返回nullptr
                     */
                    MYSQL_ROW row = mysql_fetch_row(result);
                    res.addHeader("Content-Type", "text/html; charset=utf-8");
                    if (row != nullptr) {
                        std::string db_password(row[0]);
                        if (password == db_password) {
                            // 登陆成功后，生成一个生命周期为30分钟的随机字符串作为token发给浏览器
                            std::string token = generateSecureCookieToken(32);
                            redisContext *redis;
                            RedisConnRAII guard(&redis, &RedisConnPool::Instance());
                            redisReply *reply = (redisReply*)redisCommand(redis, "SET %s %s EX %d", token.c_str(), username.c_str(), 1800);
                            if (reply == nullptr || reply->type == REDIS_REPLY_ERROR) {
                                LOG_ERROR << "Redis 密码验证失败: " << reply->str;
                                freeReplyObject(reply);
                                res.setStatusCode(500, "Internal Server Error");
                                res.setBody("Redis 密码验证失败");
                                return;
                            }
                            freeReplyObject(reply);

                            // 将 token 作为 Cookie 发给浏览器
                            res.addHeader("Set-Cookie", ("token=" + token + "; Max-Age=1800; HttpOnly; Path=/; SameSite=Lax").c_str());
                            
                            res.setStatusCode(200, "OK");
                            res.setBody("<h1>尊贵的 " + username + "，欢迎回来！访问我们的主页<a href='/index'>主页</a></h1>");
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
                std::string token = req.getCookie("token");
                if (token != "") { // 已经有一个令牌了，检查一下是否是已经登陆的用户
                    redisContext* redis;
                    RedisConnRAII guard(&redis, &RedisConnPool::Instance());
                    redisReply* reply = (redisReply*)redisCommand(redis, "GET %s", token.c_str());
                    if (reply != nullptr && reply->type == REDIS_REPLY_STRING) {
                        res.setStatusCode(200, "OK");
                        res.addHeader("Content-Type", "text/html; charset=utf-8");
                        res.setBody("<h1>尊贵的 " + std::string(reply->str) + "，欢迎回来！访问我们的主页<a href='/index'>主页</a></h1>");
                        return;
                    }
                    freeReplyObject(reply);
                }

                // 说明令牌已经过期了或者是一个无效令牌，继续执行后续的指令
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
            std::string token = req.getCookie("token");

            // 若 token 不存在，则返回 401
            if (token.empty()) {
                res.setStatusCode(401, "Unauthorized");
                res.addHeader("Content-Type", "text/html; charset=utf-8");
                res.setBody("<h1>请先登录 <a href='/login'>登录</a></h1>");
                return;
            }

            // 验证 token 是否有效
            redisContext *redis;
            RedisConnRAII guard(&redis, &RedisConnPool::Instance());
            redisReply *reply = (redisReply*)redisCommand(redis, "GET %s", token.c_str());
            
            if (reply != nullptr && reply->type == REDIS_REPLY_STRING) {
                // 这是一个合法登陆用户
                std::string current_user = reply->str;
                freeReplyObject(reply);

                LOG_INFO << "用户 " << current_user << " 访问了 index 页面";
                if (res.setFile("/home/bazinga/ServerPractice/Html/index.html")) {
                    res.addHeader("Content-Type", "text/html; charset=utf-8");
                } else {
                    res.setStatusCode(404, "Not Found");
                    res.addHeader("Content-Type", "text/html; charset=utf-8");
                    res.setBody("<h1 style='color:red;'>404 找不到页面</h1>");
                }
            } else {
                if (reply) {
                    LOG_ERROR << "Redis token验证失败: " << reply->str;
                    freeReplyObject(reply);
                } 
                res.setStatusCode(401, "Unauthorized");
                res.addHeader("Content-Type", "text/html; charset=utf-8");
                res.setBody("<h1>登录已过期或无效，请登录 <a href='/login'>登录</a></h1>");
            }
        } else if (req.getPath() == "/logout") {
            std::string token = req.getCookie("token");
            if (!token.empty()) {
                redisContext* redis;
                RedisConnRAII guard(&redis, &RedisConnPool::Instance());
                redisReply* reply = (redisReply*)redisCommand(redis, "DEL %s", token.c_str());
                if (reply != nullptr && reply->type == REDIS_REPLY_INTEGER) {
                    LOG_INFO << "用户退出登录，token 已删除";
                }
                freeReplyObject(reply);
            }
            
            // 告诉浏览器把这个cookie删掉，生命周期设置为0
            res.addHeader("Set-Cookie", "token=; Max-Age=0; HttpOnly; Path=/; SameSite=Lax");

            // 302 重定向到登录页面
            res.setStatusCode(302, "Found");
            res.addHeader("Location", "/login");
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