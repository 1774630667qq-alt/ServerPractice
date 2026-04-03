/*
 * @Author: Zhang YuHua 1774630667@qq.com
 * @Date: 2026-04-02 21:02:38
 * @LastEditors: Zhang YuHua 1774630667@qq.com
 * @LastEditTime: 2026-04-03 17:17:52
 * @FilePath: /ServerPractice/include/SqlConnPool.hpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置
 * 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#pragma once

#include <mysql/mysql.h>
#include <queue>
#include <mutex>
#include <condition_variable>

namespace MyServer {

/**
 * @brief 数据库连接池类
 * @details 负责在服务器启动时预先建立若干个 MySQL 连接，避免高并发下频繁建连断连的巨大开销。
 * 采用 C++11 的 Meyers 单例模式，保证全局唯一且绝对的线程安全。
 */
class SqlConnPool {
public:
    /**
     * @brief 获取全局唯一的连接池实例 (单例模式)
     * @details 利用 C++11 局部静态变量的特性，不仅实现了懒汉式加载，还由编译器底层保证了绝对的线程安全，无需加锁。
     * @return SqlConnPool& 返回连接池的单例引用
     */
    static SqlConnPool& Instance() {
        static SqlConnPool connPool;
        return connPool;
    }

    /**
     * @brief 初始化连接池 (在 main 函数服务器启动前调用)
     * @param host     数据库服务器 IP 地址 (如 "127.0.0.1" 或 "localhost")
     * @param port     数据库端口号 (MySQL 默认 3306)
     * @param user     数据库登录用户名 (如 "root")
     * @param pwd      数据库登录密码
     * @param dbName   要连接的具体数据库名
     * @param connSize 预先建立的连接数量 (即池子的大小，建议 10~20)
     */
    void Init(const char* host, int port,
              const char* user, const char* pwd, 
              const char* dbName, int connSize);

    /**
     * @brief 从池子中获取一个空闲的数据库连接
     * @return MYSQL* 可用的数据库连接指针
     */
    MYSQL* GetConn();

    /**
     * @brief 归还一个数据库连接回池子
     * @param conn 用完的数据库连接指针
     */
    void FreeConn(MYSQL* conn);

    /**
     * @brief 关闭连接池并销毁所有连接
     * @details 内部逻辑提示：
     */
    void ClosePool();

    /**
     * @brief 获取当前池子中空闲连接的数量
     * @return int 空闲连接数
     */
    int GetFreeConnCount() { return freeCount_; };

private:
    // ---------------- 单例模式核心防线 ----------------
    
    /**
     * @brief 构造函数私有化
     * @details 剥夺外部通过 new 或在栈上创建对象的权力
     */
    SqlConnPool() : useCount_(0), freeCount_(0), isClose_(false), isInitialized_(false) {}
    
    /**
     * @brief 析构函数私有化
     * @details 当程序退出，局部静态变量自动销毁时触发，内部应调用 ClosePool()
     */
    ~SqlConnPool() { ClosePool(); }

    /**
     * @brief 禁用拷贝构造和赋值操作符
     * @details 防止外部通过拷贝破坏单例的唯一性 (C++11 = delete 语法)
     */
    SqlConnPool(const SqlConnPool&) = delete; 
    SqlConnPool& operator=(const SqlConnPool&) = delete;

    // ---------------- 核心资源与状态变量 ----------------

    int MAX_CONN_;   ///< 连接池允许的最大连接数
    int useCount_;   ///< 当前已经被借出使用的连接数
    int freeCount_;  ///< 当前池子里还剩的空闲连接数

    bool isClose_;   ///< 标志位：表示连接池是否已关闭
    bool isInitialized_; ///< 标志位：表示连接池是否已初始化

    std::queue<MYSQL*> connQue_;   ///< 存放数据库连接的核心队列 (池子的实体)
    
    std::mutex mtx_;               ///< 互斥锁：保护对 connQue_ 和计数器的并发访问
    std::condition_variable cond_; ///< 条件变量：当池子为空时，用来阻塞和唤醒请求连接的线程
};

} // namespace MyServer