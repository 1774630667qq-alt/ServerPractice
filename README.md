# ServerPractice

一个基于 C++11 编写的高性能并发 Web 服务器，专注于底层网络架构与高并发处理。

## 🌟 项目特性

*   **Reactor 模式 & Epoll 多路复用**：采用非阻塞 I/O 和 Epoll ET 模式，结合主从多线程 Reactor 模型，高效处理海量并发连接。
*   **线程池架构 (ThreadPool)**：使用 C++11 标准库实现的高效线程池，避免频繁创建和销毁线程带来的开销，提升系统整体响应速度。
*   **双数据库连接池 (MySQL & Redis)**：
    *   **MySQL 连接池**：基于单例模式和 RAII 机制管理数据库连接，保障高并发场景下安全、高效的 SQL 读写。
    *   **Redis 连接池**：深度集成 `hiredis` 库，利用 RAII 封装 Redis 连接，支持高性能的 Token 缓存、状态存储及快速验证。
*   **HTTP 协议解析 (HttpParser)**：实现了一套轻量级的 HTTP 状态机解析器，支持解析 HTTP/1.1 请求（GET, POST 等），并能灵活构建标准的 HTTP 响应。
*   **路由与鉴权系统**：
    *   内置灵活的路由分发机制，支持快速绑定业务逻辑。
    *   实现了基于 Cookie 和 Redis Token 的全流程认证机制（包含登录鉴权、路由保护和安全退出）。
*   **异步日志系统 (AsyncLogging)**：基于双缓冲 (Double Buffering) 机制实现的异步日志记录器，支持按日志级别过滤和线程安全输出，将磁盘 I/O 阻塞对主业务逻辑的影响降至最低。

## 🛠️ 核心技术栈

*   **编程语言**：C++11
*   **网络编程**：Socket 编程, Epoll, 非阻塞 I/O, Reactor 模型
*   **并发编程**：`std::thread`, `std::mutex`, `std::condition_variable`, `std::unique_lock`, 线程池
*   **数据库组件**：MySQL C API, Redis (`hiredis`)
*   **构建工具**：CMake

## 📂 核心目录结构

```text
ServerPractice/
├── CMakeLists.txt      # CMake 构建配置
├── include/            # 头文件目录 (各种核心类的声明)
│   ├── TcpServer.hpp   # TCP 服务器核心
│   ├── EventLoop.hpp   # 事件循环 (Reactor 核心)
│   ├── HttpServer.hpp  # HTTP 服务器封装
│   ├── SqlConnPool.hpp # MySQL 连接池
│   ├── RedisConnPool.hpp# Redis 连接池
│   └── ...
├── src/                # 源文件目录 (核心业务逻辑实现)
│   ├── main.cpp        # 程序的入口，初始化服务器和路由配置
│   ├── TcpServer.cpp
│   ├── HttpParser.cpp  
│   └── ...
├── Html/               # 静态资源存放路径 (如 index.html)
└── build/              # 编译生成目录
```

## 🚀 快速启动

### 1. 环境依赖
确保您的 Linux 系统中已安装以下组件：
*   GCC/G++ 编译器 (支持 C++11)
*   CMake
*   MySQL 数据库及 `libmysqlclient-dev`
*   Redis 数据库及 `libhiredis-dev`

### 2. 构建项目
```bash
mkdir build
cd build
cmake ..
make
```

### 3. 运行服务器
```bash
./main
```
*启动前，请确保本机的 MySQL 和 Redis 服务已正常运行，且账户密码、端口配置与 `src/main.cpp` 中的初始化参数一致。*

## 📝 学习与开发总结
本项目主要作为学习 C++ 后端高并发开发的实战练习，重点突破了：
1. 底层事件驱动网络框架的搭建与指针内存排查。
2. RAII 机制在实际资源管理（数据库连接、内存对象）中的应用。
3. 高并发场景下竞态条件的排查以及段错误（核心转储）的分析与修复。
