#include <sys/socket.h> // 提供 socket 函数及数据结构
#include <netinet/in.h> // 提供 IPv4 的 sockaddr_in 结构体
#include <arpa/inet.h>  // 提供 IP 地址转换函数
#include <unistd.h>     // 提供 close 函数
#include <iostream>
#include <cstring>      // 提供 memset

int main () {
    // socket()
    /**
     * @brief 创建一个端点用于通信
     * @signature int socket(int domain, int type, int protocol);
     * @param domain 通信协议族，例如 AF_INET 表示 IPv4，AF_INET6 表示 IPv6
     * @param type 套接字类型，例如 SOCK_STREAM 表示面向连接的字节流(TCP)，SOCK_DGRAM 表示数据报(UDP)
     * @param protocol 具体协议，通常写 0 让系统根据 domain 和 type 选择默认协议
     * @return 成功返回一个新的文件描述符(非负整数)，失败返回 -1
     */
    // 申请一个 IPv4 的 TCP socket
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == -1) {
        std::cerr << "Socket 创建失败!" << std::endl;
        return -1;
    }
    std::cout << "Socket 创建成功，fd: " << listen_fd << std::endl;

    // bind()
    /**
     * @brief 将一个名字(IP与端口)与套接字绑定到一起
     * @signature int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
     * @param sockfd 由 socket() 创建产生的文件描述符
     * @param addr 指向通用地址结构体 sockaddr 的指针，包含要绑定的 IP 和端口（通常用 sockaddr_in 强转传入）
     * @param addrlen addr 结构体的实际内存大小，通常为 sizeof(struct sockaddr_in)
     * @return 成功返回 0，失败返回 -1
     */
    /**
     * @brief sockaddr_in 是专门针对 IPv4 地址的结构体，用于保存 IP 地址和端口等信息
     *   - sin_family: 协议族（AF_INET 表示 IPv4）
     *   - sin_port: 16 位端口号（需转换为网络字节序，如 htons()）
     *   - sin_addr: IPv4 地址结构体，包含 s_addr 用于保存 32 位 IP 地址（需转换为网络字节序，如 htonl()）
     *   - sin_zero: 8 字节填充，用于保证与通用 sockaddr 结构体大小一致，方便强制类型转换
     */
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr)); // 先清零
    server_addr.sin_family = AF_INET;             // IPv4
    // htons(8080) 将主机字节序(小端)转换为网络字节序(大端)
    server_addr.sin_port = htons(8080);           
    // INADDR_ANY 表示监听本机的所有网卡 IP (即 0.0.0.0)
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); 

    // 强转并绑定
    if (bind(listen_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        std::cerr << "Bind 绑定失败!" << std::endl;
        return -1;
    }

    // listen()
    /**
     * @brief 监听套接字上的连接，将套接字标记为被动状态
     * @signature int listen(int sockfd, int backlog);
     * @param sockfd 已经被 bind() 绑定了地址的套接字文件描述符
     * @param backlog 允许在未决连接队列中排队的最大连接数（即全连接队列长度）
     * @return 成功返回 0，失败返回 -1
     */
    if (listen(listen_fd, 128) == -1) {
        std::cerr << "Listen 失败!" << std::endl;
        return -1;
    }
    std::cout << "服务器启动，正在监听 8080 端口..." << std::endl;

    // accept()
    /**
     * @brief 接受一个套接字上的连接，返回一个新的已连接套接字文件描述符
     * @signature int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
     * @param sockfd 处于 listen 状态的监听套接字文件描述符
     * @param addr 传出参数，指向用于保存客户端地址信息(IP、端口)的结构体的指针
     * @param addrlen 传入传出参数，传入时为 addr 的大小，传出时为客户端实际地址大小
     * @return 成功返回一个新的文件描述符，专门用于与刚连接上的客户端进行数据通信；失败返回 -1
     */
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    // 程序会在这里卡住（阻塞），直到有客户端连上来！
    int client_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &client_len);
    if (client_fd == -1) {
        std::cerr << "Accept 失败!" << std::endl;
    } else {
        std::cout << "有新客户端连上来了！分配的通信 fd: " << client_fd << std::endl;
    }

    // recv()
    /**
     * @brief 从已连接的套接字接收消息
     * @signature ssize_t recv(int sockfd, void *buf, size_t len, int flags);
     * @param sockfd 与客户端通信的已连接套接字文件描述符(如 accept 的返回值)
     * @param buf 指向存放接收到的数据的缓冲区
     * @param len 期望接收的最大字节数（通常为 buf 的大小减一，留给字符串结束符）
     * @param flags 接收控制标志，一般设为 0 表示默认阻塞行为
     * @return 成功返回实际接收到的字节数；返回 0 表示对端(客户端)优雅地关闭了连接；返回 -1 表示出错
     */
    char buffer[1024];
    memset(buffer, 0, sizeof(buffer)); // 清空缓冲区

    // 阻塞等待客户端发消息
    ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read > 0) {
        std::cout << "收到客户端消息: " << buffer << std::endl;
    } else if (bytes_read == 0) {
        std::cout << "客户端断开连接" << std::endl;
    }

    // send() 
    /**
     * @brief 发送消息到已连接的套接字
     * @signature ssize_t send(int sockfd, const void *buf, size_t len, int flags);
     * @param sockfd 与客户端通信的已连接套接字文件描述符
     * @param buf 指向要发送的数据所在的缓冲区
     * @param len 要发送的实际字节数
     * @param flags 发送控制标志，一般设为 0 表示默认行为
     * @return 成功返回实际发送成功的字节数，失败返回 -1
     */
    // 把收到的消息原封不动发回去 (Echo)
    if (bytes_read > 0) {
        ssize_t bytes_sent = send(client_fd, buffer, bytes_read, 0);
        if (bytes_sent == -1) {
            std::cerr << "发送失败!" << std::endl;
        }
    }

    // close()
    /**
     * @brief 关闭一个文件描述符，释放相关资源
     * @signature int close(int fd);
     * @param fd 要关闭的文件描述符（套接字）
     * @return 成功返回 0，失败返回 -1
     */
    close(client_fd); // 关掉与这个客户端的通信
    // close(listen_fd); // 服务器关机时才调用
    return 0;
}