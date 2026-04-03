/*
 * @Author: Zhang YuHua 1774630667@qq.com
 * @Date: 2026-04-03 23:24:40
 * @LastEditors: Zhang YuHua 1774630667@qq.com
 * @LastEditTime: 2026-04-03 23:49:38
 * @FilePath: /ServerPractice/include/RedisConnRAII.hpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置
 * 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#pragma once
#include "RedisConnPool.hpp"

namespace MyServer {

class RedisConnRAII {
public:
    RedisConnRAII(redisContext **redis, RedisConnPool *connPool) {
        *redis = connPool->GetConn();
        redis_ = *redis;
        connpool_ = connPool;
    }

    ~RedisConnRAII() {
        if (redis_) {
        connpool_->FreeConn(redis_);
        }
    }

    private:
    redisContext *redis_;
    RedisConnPool *connpool_;
};

} // namespace MyServer