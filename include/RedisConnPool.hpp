/*
 * @Author: Zhang YuHua 1774630667@qq.com
 * @Date: 2026-04-03 23:06:54
 * @LastEditors: Zhang YuHua 1774630667@qq.com
 * @LastEditTime: 2026-04-03 23:57:13
 * @FilePath: /ServerPractice/include/RedisConnPool.hpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置
 * 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#pragma once

#include <hiredis/hiredis.h>
#include <queue>
#include <mutex>
#include <condition_variable>

namespace MyServer {

/**
 * @brief Redis 数据库连接池类
 * @details 负责在服务器启动时预先建立若干个 Redis 连接 (redisContext)
 */
class RedisConnPool {
public:
    /**
     * @brief 获取全局唯一的 Redis 连接池实例 (单例模式)
     * @details 利用 C++11 局部静态变量的特性，实现无锁的、线程安全的懒汉式加载。
     * @return RedisConnPool& 返回连接池的单例引用
     */
    static RedisConnPool& Instance() {
        static RedisConnPool pool;
        return pool;
    }

    /**
     * @brief 初始化连接池 (在 main 函数服务器启动前调用)
     * @param host     Redis 服务器 IP 地址 (通常为 "127.0.0.1")
     * @param port     Redis 端口号 (默认 6379)
     * @param pwd      Redis 密码 (如果没有设置密码，可以传 nullptr 或 "")
     * @param connSize 预先建立的连接数量 (即池子的大小)
     */
    void Init(const char* host, int port, const char* pwd, int connSize);

    /**
     * @brief 从池子中获取一个空闲的 Redis 连接
     * @return redisContext* 可用的 Redis 连接指针
     */
    redisContext* GetConn();

    /**
     * @brief 归还一个 Redis 连接回池子
     * @param context 用完的 Redis 连接指针
     */
    void FreeConn(redisContext* context);

    /**
     * @brief 关闭连接池并销毁所有连接
     */
    void ClosePool();

private:
    // ---------------- 单例模式核心防线 ----------------
    
    /**
     * @brief 构造函数私有化
     * @details 剥夺外部创建对象的权力
     */
    RedisConnPool() : MAX_CONN_(0), useCount_(0), freeCount_(0), isClosed_(false), isInit_(false) {}
    
    /**
     * @brief 析构函数私有化
     * @details 局部静态变量销毁时触发，内部应调用 ClosePool()
     */
    ~RedisConnPool() { ClosePool(); }

    /**
     * @brief 禁用拷贝构造和赋值操作符 (C++11 = delete)
     */
    RedisConnPool(const RedisConnPool&) = delete; 
    RedisConnPool& operator=(const RedisConnPool&) = delete;

    // ---------------- 核心资源与状态变量 ----------------

    int MAX_CONN_;   ///< 连接池最大容量
    int useCount_;   ///< 当前正在使用的连接数
    int freeCount_;  ///< 当前空闲的连接数
    bool isClosed_;  ///< 连接池是否已关闭的标志位，用于优雅停机
    bool isInit_;    ///< 连接池是否已初始化的标志位，用于防止重复初始化

    std::queue<redisContext*> connQue_; ///< 存放 Redis 连接的核心队列
    
    std::mutex mtx_;               ///< 互斥锁，保护并发读写
    std::condition_variable cond_; ///< 条件变量，用于队列为空时的阻塞和唤醒
};

} // namespace MyServer