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
        MYSQL *conn = mysql_init(nullptr);
        if (conn == nullptr) {
            LOG_ERROR << "MYSQL 初始化失败";
            // 出现问题，直接关闭连接池并杀死程序
            ClosePool();
            exit(EXIT_FAILURE);
        }

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