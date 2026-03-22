/*
 * @Author: Zhang YuHua 1774630667@qq.com
 * @Date: 2026-03-22 20:28:39
 * @LastEditors: Zhang YuHua 1774630667@qq.com
 * @LastEditTime: 2026-03-22 20:28:48
 * @FilePath: /ServerPractice/include/Buffer.hpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#pragma once
#include <string>

namespace MyServer {

/**
 * @brief 应用层缓冲区：解决 TCP 粘包/半包问题的神器
 */
class Buffer {
private:
    std::string buf_; // 这里用 string 极其方便，因为它自带动态扩容和各种查找方法

public:
    Buffer() = default;
    ~Buffer() = default;

    /**
     * @brief 把底层收到的数据追加到缓冲区
     */
    void append(const char* data, size_t len) {
        buf_.append(data, len);
    }

    /**
     * @brief 获取当前缓冲区里有多少字节的数据
     */
    size_t size() const {
        return buf_.size();
    }

    /**
     * @brief 提取出一条完整的数据 (这需要根据你的协议来写)
     * * 假设我们的协议是：每条消息以 \n 结尾
     * @return 如果找到了一完整消息，返回它并从 Buffer 中删掉；如果不够一条，返回空字符串
     */
    std::string extractMessage() {
        // 在缓冲区中查找 '\n' 的位置
        size_t pos = buf_.find('\n');
        
        if (pos != std::string::npos) {
            // 找到了！说明有一条完整的消息
            // 提取从头到 '\n' 的所有字符 (包含 \n)
            std::string msg = buf_.substr(0, pos + 1); 
            // 把提取走的数据从水池里删掉
            buf_.erase(0, pos + 1); 
            return msg;
        }
        
        // 没找到 \n，说明是半包，什么都不返回，继续等后续数据
        return ""; 
    }
};

} // namespace MyServer