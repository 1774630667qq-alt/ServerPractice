/*
 * @Author: Zhang YuHua 1774630667@qq.com
 * @Date: 2026-04-03 17:19:52
 * @LastEditors: Zhang YuHua 1774630667@qq.com
 * @LastEditTime: 2026-04-03 17:21:05
 * @FilePath: /ServerPractice/include/SqlConnRAII.hpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置
 * 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#pragma once
#include "SqlConnPool.hpp"

namespace MyServer {

/**
 * @brief 数据库连接的智能守卫 (RAII)
 * @details 在构造时自动从池中获取连接，在析构时自动归还连接。
 */
class SqlConnRAII {
public:
    /**
     * @brief 构造函数：获取连接
     * @param sql 传入一个指针的引用，用于接收分配到的 MYSQL 连接
     * @param connPool 传入连接池实例
     */
    SqlConnRAII(MYSQL** sql, SqlConnPool* connPool) {
        *sql = connPool->GetConn();
        sql_ = *sql;
        connpool_ = connPool;
    }
    
    /**
     * @brief 析构函数：自动归还连接，绝不泄露！
     */
    ~SqlConnRAII() {
        if (sql_) {
            connpool_->FreeConn(sql_);
        }
    }
    
private:
    MYSQL *sql_;
    SqlConnPool* connpool_;
};

} // namespace MyServer