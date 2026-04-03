/*
 * @Author: Zhang YuHua 1774630667@qq.com
 * @Date: 2026-04-03 16:36:34
 * @LastEditors: Zhang YuHua 1774630667@qq.com
 * @LastEditTime: 2026-04-03 17:43:16
 * @FilePath: /ServerPractice/src/SqlConnPoolsrc.cpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置
 * 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include "SqlConnPool.hpp"
#include "Logger.hpp"
#include <cstdlib>
#include <mysql.h>

namespace MyServer {
void SqlConnPool::Init(const char *host, int port, const char *user, const char *pwd,
            const char *dbName, int connSize) {
    std::lock_guard<std::mutex> lock(mtx_);
    // 防止重复初始化
    if (isInitialized_) {
        LOG_WARNING << "SqlConnPool 已经执行过初始化，跳过此次调用。";
        return;
    }
    isInitialized_ = true;

    MAX_CONN_ = connSize;
    for (int i = 0; i < connSize; i++) {
        /**
         * @brief 分配或初始化与MySQL服务器连接相关的MYSQL对象
         * @signature MYSQL *mysql_init(MYSQL *mysql);
         * @param mysql 若为nullptr，则分配、初始化并返回一个新对象。否则，将初始化该对象并返回它的地址。
         * @return 成功返回一个被初始化的MYSQL*句柄，如果在内存不足以分配新对象的情况下返回nullptr。
         */
        MYSQL *conn = mysql_init(nullptr);
        if (conn == nullptr) {
            LOG_ERROR << "MYSQL 初始化失败";
            // 出现问题，直接关闭连接池并杀死程序
            ClosePool();
            exit(EXIT_FAILURE);
        }

        /**
         * @brief 尝试与运行在host上的MySQL数据库引擎建立连接
         * @signature MYSQL *mysql_real_connect(MYSQL *mysql, const char *host, const char *user, const char *passwd, const char *db, unsigned int port, const char *unix_socket, unsigned long client_flag);
         * @param mysql 已初始化的MYSQL结构体句柄
         * @param host 主机名或IP地址
         * @param user MySQL登录用户名
         * @param pwd 用户密码
         * @param dbName 要使用的数据库名
         * @param port MySQL服务器端口号
         * @param unix_socket 通常为nullptr，如果非空则指定使用的套接字或命名管道
         * @param client_flag 客户端标志，通常为0
         * @return 成功返回MYSQL*连接句柄，失败返回nullptr
         */
        if (mysql_real_connect(conn, host, user, pwd, dbName, port, nullptr, 0) == nullptr) {
            LOG_ERROR << "MYSQL 连接失败";
            // 出现问题，直接关闭连接池和mysql句柄并杀死程序
            mysql_close(conn);
            ClosePool();
            exit(EXIT_FAILURE);
        }

        connQue_.push(conn);
        freeCount_++;
    }
}

MYSQL* SqlConnPool::GetConn() {
    std::unique_lock<std::mutex> lock(mtx_);
    cond_.wait(lock, [this]{ return !connQue_.empty() || isClose_; });
    if (isClose_) {
        lock.unlock();
        return nullptr;
    }
    MYSQL* conn = connQue_.front();
    connQue_.pop();
    useCount_++;
    freeCount_--;
    return conn;
}

void SqlConnPool::FreeConn(MYSQL* conn) {
    std::unique_lock<std::mutex> lock(mtx_);
    if (isClose_) { // 如果连接池已关闭，直接销毁连接
        lock.unlock();
        mysql_close(conn);
        return;
    }
    connQue_.push(conn);
    useCount_--;
    freeCount_++;
    cond_.notify_one();
}

void SqlConnPool::ClosePool() {
    std::lock_guard<std::mutex> lock(mtx_);
    isClose_ = true;

    while (!connQue_.empty()) {
        MYSQL* conn = connQue_.front();
        connQue_.pop();
        mysql_close(conn);
    }

    useCount_ = 0;
    freeCount_ = 0;
    // 叫醒所有等待的线程
    cond_.notify_all();
}

} // namespace MyServer