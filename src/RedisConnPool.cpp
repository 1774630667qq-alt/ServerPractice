/*
 * @Author: Zhang YuHua 1774630667@qq.com
 * @Date: 2026-04-03 23:24:58
 * @LastEditors: Zhang YuHua 1774630667@qq.com
 * @LastEditTime: 2026-04-04 00:04:31
 * @FilePath: /ServerPractice/src/RedisConnPool.cpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置
 * 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include "RedisConnPool.hpp"
#include "Logger.hpp"
#include <cstdlib>

namespace MyServer {
void RedisConnPool::Init(const char* host, int port, const char* pwd, int connSize) {
    std::lock_guard<std::mutex> lock(mtx_);

    if (isInit_) {
        LOG_WARNING << "RedisConnPool 已经执行过初始化，跳过此次调用。";
        return;
    }
    isInit_ = true;
    
    MAX_CONN_ = connSize;
    for (int i = 0; i < connSize; ++i) {
        /**
         * @brief 用于创建一个与 Redis 服务器的连接上下文
         * @signature redisContext *redisConnect(const char *ip, int port)
         * @param ip Redis 服务器的 IP 地址
         * @param port Redis 服务器的监听端口
         * @return 成功时返回 redisContext 指针，失败时返回 nullptr 或包含错误信息的上下文
         */
        redisContext* conn = redisConnect(host, port);
        if (conn == nullptr || conn->err) {
            if (conn -> err) {
                redisFree(conn);
                LOG_ERROR << "Redis 连接失败: " << conn->errstr;
            } else {
                LOG_ERROR << "Redis 连接失败";
            }
            // 出现问题，直接关闭连接池并杀死程序
            ClosePool();
            exit(EXIT_FAILURE);
        }
        if (pwd != nullptr) {
            /**
             * @brief 执行 Redis 命令并返回执行结果
             * @signature void *redisCommand(redisContext *c, const char *format, ...)
             * @param c 与 Redis 建立的连接上下文
             * @param format Redis 命令格式字符串，支持类似于 printf 的格式化参数
             * @return 成功时返回包含执行结果的 redisReply 指针，失败时返回 nullptr
             */
            redisReply* reply = (redisReply*)redisCommand(conn, "AUTH %s", pwd);
            if (reply == nullptr || reply->type == REDIS_REPLY_ERROR) {
                if (reply == nullptr) {
                    LOG_ERROR << "Redis 密码验证失败";
                } else {
                    LOG_ERROR << "Redis 密码验证失败: " << reply->str;
                    freeReplyObject(reply);
                }
                // 出现问题，直接关闭连接池并杀死程序
                ClosePool();
                exit(EXIT_FAILURE);
            }
            /**
             * @brief 释放 redisCommand 返回的 redisReply 对象占据的内存
             * @signature void freeReplyObject(void *reply)
             * @param reply 需要被释放的 redisReply 对象指针
             * @return 无返回值
             */
            freeReplyObject(reply);
        }
        connQue_.push(conn);
        freeCount_++;
    }
}  

redisContext* RedisConnPool::GetConn() {
    std::unique_lock<std::mutex> lock(mtx_);
    cond_.wait(lock, [this]{ return !connQue_.empty() || isClosed_; });
    if (isClosed_) {
        lock.unlock();
        return nullptr;
    }
    redisContext* conn = connQue_.front();
    connQue_.pop();
    useCount_++;
    freeCount_--;
    return conn;
}

void RedisConnPool::FreeConn(redisContext* conn) {
    std::unique_lock<std::mutex> lock(mtx_);
    
    if (conn == nullptr) {
        return;
    }

    if (isClosed_) {
        lock.unlock();
        /**
         * @brief 断开与 Redis 的连接，并释放 redisContext 及其内部相关联的内存
         * @signature void redisFree(redisContext *c)
         * @param c 需要被断开连接和释放内存的 redisContext 上下文指针
         * @return 无返回值
         */
        redisFree(conn);
        return;
    }
    connQue_.push(conn);
    useCount_--;
    freeCount_++;
    cond_.notify_one();
}

void RedisConnPool::ClosePool() {
    std::lock_guard<std::mutex> lock(mtx_);
    isClosed_ = true;

    while (!connQue_.empty()) {
        redisContext* conn = connQue_.front();
        connQue_.pop();
        redisFree(conn);
    }

    useCount_ = 0;
    freeCount_ = 0;
    // 叫醒所有等待的线程
    cond_.notify_all();
}
}