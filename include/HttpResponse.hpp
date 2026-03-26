/*
 * @Author: Zhang YuHua 1774630667@qq.com
 * @Date: 2026-03-26 16:50:09
 * @LastEditors: Zhang YuHua 1774630667@qq.com
 * @LastEditTime: 2026-03-26 16:51:14
 * @FilePath: /ServerPractice/include/HttpResponse.hpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#pragma once
#include <string>
#include <unordered_map>

namespace MyServer {

/**
 * @brief HTTP 响应实体类：业务层只需要设置状态码和内容，它会自动组装成标准的 HTTP 报文
 */
class HttpResponse {
private:
    std::string version_;           ///< 协议版本，默认 "HTTP/1.1"
    int statusCode_;                ///< 状态码，如 200, 404
    std::string statusMessage_;     ///< 状态信息，如 "OK", "Not Found"
    
    std::unordered_map<std::string, std::string> headers_; ///< 响应头
    std::string body_;              ///< 响应体 (HTML, JSON 等)

public:
    // 默认构造一个成功的 200 响应
    HttpResponse() : version_("HTTP/1.1"), statusCode_(200), statusMessage_("OK") {
        // 默认加上长连接关闭的请求头，避免浏览器转圈
        headers_["Connection"] = "close"; 
    }
    ~HttpResponse() = default;

    // --- 设置状态和内容 ---
    void setStatusCode(int code, const std::string& msg) {
        statusCode_ = code;
        statusMessage_ = msg;
    }
    
    void addHeader(const std::string& key, const std::string& value) {
        headers_[key] = value;
    }
    
    void setBody(const std::string& body) {
        body_ = body;
        // 极其智能的一步：只要设置了 Body，自动计算并加上 Content-Length！
        // 彻底杜绝上次浏览器无限转圈的惨剧！
        headers_["Content-Length"] = std::to_string(body_.size());
    }

    // --- 核心序列化功能 ---
    /**
     * @brief 将当前的 C++ 对象，打包成符合 HTTP 协议的纯文本字符串，准备发给底层 TCP
     */
    std::string assemble() const {
        std::string response;
        // 1. 拼装响应行
        response += version_ + " " + std::to_string(statusCode_) + " " + statusMessage_ + "\r\n";
        
        // 2. 拼装响应头
        for (const auto& header : headers_) {
            response += header.first + ": " + header.second + "\r\n";
        }
        
        // 3. 拼装空行 (极其致命的分界线)
        response += "\r\n";
        
        // 4. 拼装响应体
        response += body_;
        
        return response;
    }
};

} // namespace MyServer