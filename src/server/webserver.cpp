/***********************************************
 * FileName    : webserver.cpp
 * Description : the method definition of WebServer class
 * 
 * Feature     :
 * 
 * Author      : JasonGe
 * Created on  : 2025/03/26
************************************************/

#include "webserver.h"
#include <openssl/ssl.h>
#include <openssl/err.h>

WebServer::WebServer(int port, int trigMode, int timeoutMS, bool OptLinger,
              int sqlPort, const char* sqlUser, const char* sqlPwd,
              const char* dbName, int connPoolNum, int threadNum,
              bool openLog, int logLevel, int logQueSize, 
              bool useSSL, const char* certPath, const char* keyPath):
              port_(port), openLinger_(OptLinger), timeoutMS_(timeoutMS), isClose_(false),
              timer_(new HeapTimer()), threadpool_(new ThreadPool(threadNum)), epoller_(new Epoller()),
              useSSL_(useSSL), certPath_(certPath), keyPath_(keyPath), sslCtx_(nullptr)
{
    srcDir_ = getcwd(nullptr, 256);
    assert(srcDir_);  // 获取当前工作目录的绝对路径
    strncat(srcDir_, "/resources/", 16);    // 附加到根目录末尾
    HttpConn::userCount = 0;        // 静态成员变量初始化
    HttpConn::srcDir = srcDir_;
    SqlConnPool::Instance()->Init("localhost", sqlPort, sqlUser, sqlPwd, dbName, connPoolNum); // 初始化数据库连接池

    if (useSSL_) {
        InitSSL_();
    }
    
    InitEventMode_(trigMode);
    if (!InitSocket_()) {isClose_ = true;}

    if (openLog) {   // 记录参数的日志信息
        Log::Instance()->init(logLevel, "./log", ".log", logQueSize);  // 创建日志实例，并初始化
        if (isClose_) {LOG_ERROR(MODULE_WEBSERVER, "==========Server init error!==========");}
        else {
            LOG_INFO(MODULE_WEBSERVER, "==========Server init==========");  // 记录服务器初始化参数
            LOG_INFO(MODULE_WEBSERVER, "Port:%d, OpenLinger:%s", port_, OptLinger?"true":"false");
            LOG_INFO(MODULE_WEBSERVER, "Listen Mode:%s, OpenConn Mode:%s",
                    (listenEvent_ & EPOLLET ? "ET" : "LT"),
                    (connEvent_ & EPOLLET ? "ET" : "LT"));  // 记录事件触发模式

            LOG_INFO(MODULE_WEBSERVER, "LogSys level:%d", logLevel);
            LOG_INFO(MODULE_WEBSERVER, "srcDir:%s", HttpConn::srcDir);
            LOG_INFO(MODULE_WEBSERVER, "SqlConnPool num: %d, ThreadPool num: %d", connPoolNum, threadNum);
            if (useSSL_) {
                LOG_INFO(MODULE_WEBSERVER, "SSL enabled, Cert: %s, Key: %s", certPath_, keyPath_);
            }
        }
    }
}

WebServer::~WebServer() {
    close(listenFd_);
    isClose_ = true;
    free(srcDir_);
    SqlConnPool::Instance()->ClosePool();
    if (useSSL_) {
        CleanupSSL_();
    }
}

void WebServer::InitEventMode_(int trigMode) {
    listenEvent_ = EPOLLRDHUP;    // 监听socket的epoll事件要包括对端是否关闭socket连接
    connEvent_ = EPOLLONESHOT | EPOLLRDHUP;
    switch (trigMode)
    {
    case 0:
        break;
    case 1:
        connEvent_ |= EPOLLET;
        break;
    case 2:
        listenEvent_ |= EPOLLET;
        break;
    case 3:
        listenEvent_ |= EPOLLET;
        connEvent_ |= EPOLLET;
        break;
    default:
        listenEvent_ |= EPOLLET;
        connEvent_ |= EPOLLET;
        break;
    }
    HttpConn::isET = (connEvent_ & EPOLLET);
}

void WebServer::Start() {
    int timeMS = -1;  /* epoll wait timeout == -1 无事件将阻塞 */
    if (!isClose_) {LOG_INFO(MODULE_WEBSERVER, "========== Server start ==========");}
    while (!isClose_) {
        if (timeoutMS_ > 0) {
            timeMS = timer_->GetNextTick(); // 每次都要检查是否超时
        }
        int eventCnt = epoller_->Wait(timeMS);  // 等待就绪事件epoll_wait
        for (int i = 0; i < eventCnt; i++) {
            /*处理事件*/
            int fd = epoller_->GetEventFd(i);   // 获取就绪的连接
            uint32_t events = epoller_->GetEvents(i);  // 获取就绪的事件
            if (fd == listenFd_) {  // 有新连接到来
                DealListen_();
            }
            else if (events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) { // 如果对端关闭连接
                assert(users_.count(fd) > 0);   // 断言该连接socket的任务处理对象必须存在
                CloseConn_(&users_[fd]);    // 关闭socket连接
            }
            else if (events & EPOLLIN) {   // 如果是读就绪事件，断言对应任务存在，并执行读socket操作
                assert(users_.count(fd) > 0);
                DealRead_(&users_[fd]);
            }
            else if (events & EPOLLOUT) {  // 如果是写就绪事件，断言对应任务存在，并执行写socket操作
                assert(users_.count(fd) > 0);
                DealWrite_(&users_[fd]);
            } else {
                LOG_ERROR(MODULE_WEBSERVER, "Unexpected event");
            }
        }
    }
}

/* 给客户发送报错消息 */
void WebServer::SendError_(int fd, const char* info) {
    assert(fd > 0);
    int ret = send(fd, info, strlen(info), 0);
    if (ret < 0) {
        LOG_WARN(MODULE_WEBSERVER, "Send error to client[%d] error!", fd);
    }
    close(fd);
}

/* 关闭客户连接，删除epoll事件，关闭连接*/
void WebServer::CloseConn_(HttpConn* client) {  
    assert(client);
    LOG_INFO(MODULE_WEBSERVER, "Client[%d] quit!", client->GetFd());
    epoller_->DelFd(client->GetFd());
    client->Close();
}

/**
 * 处理新客户端连接
 * 
 * 当有新客户端连接时，执行以下操作：
 * 1. 检查SSL配置和上下文
 * 2. 如果是SSL模式，执行SSL握手
 * 3. 初始化HTTP连接对象
 * 4. 设置连接超时定时器
 * 5. 注册到epoll事件循环
 * 
 * @param fd 客户端socket文件描述符
 * @param addr 客户端地址信息
 */
void WebServer::AddClient_(int fd, sockaddr_in addr) {
    // 如果启用了SSL但SSL上下文未初始化，则拒绝连接
    if (useSSL_ && !sslCtx_) {
        LOG_ERROR(MODULE_WEBSERVER, "SSL context not initialized");
        return;
    }
    
    assert(fd > 0);

    SSL* ssl = nullptr;
    if (useSSL_) {
        // 为SSL连接创建新的SSL对象
        ssl = SSL_new(sslCtx_);
        
        // 将socket文件描述符与SSL对象关联
        SSL_set_fd(ssl, fd);
        
        // 执行SSL握手，建立加密连接
        // SSL_accept()会与客户端协商加密算法和密钥
        if (SSL_accept(ssl) <= 0) {
            // SSL握手失败，清理资源并拒绝连接
            SSL_free(ssl);
            return;
        }
    }
    
    // 初始化HTTP连接对象，传入SSL对象（如果启用SSL）
    users_[fd].init(fd, addr, ssl);
    
    // 如果设置了连接超时，添加定时器
    if (timeoutMS_ > 0) {
        timer_->add(fd, timeoutMS_, std::bind(&WebServer::CloseConn_, this, &users_[fd]));
    }
    
    // 将socket注册到epoll事件循环，监听读事件
    epoller_->AddFd(fd, EPOLLIN | connEvent_);
    
    // 设置socket为非阻塞模式
    SetFdNonblock(fd);
    
    LOG_INFO(MODULE_WEBSERVER, "Client[%d] in!", users_[fd].GetFd());
}

/* 接受客户连接请求 */
void WebServer::DealListen_() {
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    do {
        int fd = accept(listenFd_, (struct sockaddr*)& addr, &len);
        if (fd < 0) return;  // 当所有客户连接请求都被处理，此时会返回，也退出了while循环
        else if (HttpConn::userCount >= MAX_FD) {
            SendError_(fd, "Server busy!");
            LOG_WARN(MODULE_WEBSERVER, "Clients is full!");
            return;
        }
        AddClient_(fd, addr);
    } while (listenEvent_ & EPOLLET);  // 如果监听socket设置了ET模式，一次性接受所有客户连接请求
}

/* 当有客户数据来时把读数据任务交给线程池 */
void WebServer::DealRead_(HttpConn* client) {
    assert(client);
    ExtentTime_(client);
    threadpool_->AddTask(std::bind(&WebServer::OnRead_, this, client));
}

/* 把写数据的任务交给线程池 */
void WebServer::DealWrite_(HttpConn* client) {
    assert(client);
    ExtentTime_(client);
    threadpool_->AddTask(std::bind(&WebServer::OnWrite_, this, client));
}

/* 扩充定时时间,当有新任务发生时就重置定时时间 */
void WebServer::ExtentTime_(HttpConn* client) {
    assert(client);
    if(timeoutMS_ > 0) { timer_->adjust(client->GetFd(), timeoutMS_); }
}

/* 读socket连接上的数据，被插入到线程池的任务请求队列 */
void WebServer::OnRead_(HttpConn* client) {
    assert(client);
    int ret = -1;
    int readErrno = 0;
    ret = client->read(&readErrno);  // 将连接socket上的数据读入缓冲区
    if (ret <= 0 && readErrno != EAGAIN) { // 如果不是因为阻塞导致读失败，则关闭连接
        CloseConn_(client);
        return;
    }
    OnProcess(client); 
}

/* 读缓冲区中数据的处理程序，根据处理结果决定是否监听socket写就绪事件 */
void WebServer::OnProcess(HttpConn* client) {
    if(client->process()) {  // 从读缓冲区分析请求，并写应答报文
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT); // 返回数据准备好了，则监听socket写就绪事件
    } else {
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLIN);  // 分析失败，继续监听socket读就绪事件
    }
}

/* 往连接soceket上写数据*/
void WebServer::OnWrite_(HttpConn* client) {
    assert(client);
    int ret = -1;
    int writeErrno = 0;
    ret = client->write(&writeErrno);  // 发送数据给客户
    if (client->ToWriteBytes() == 0) {
        // 传输完成
        if (client->IsKeepAlive()) {
            OnProcess(client);
            return;
        }
    }
    else if (ret < 0) {
        if (writeErrno == EAGAIN) {  // 可能是TCP写缓冲已满
            // 继续监听socket写就绪事件
            epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);
            return;
        }
    }
    CloseConn_(client);
}

/* 创建监听socket*/
bool WebServer::InitSocket_() {
    int ret;
    struct sockaddr_in addr;
    if (port_ > 65535 || port_ < 1024) {  // port_是本地服务器的监听端口
        LOG_ERROR(MODULE_WEBSERVER, "Port:%d error", port_);
        return false;
    }
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    struct linger optLinger = {0};
    if (openLinger_) {
        /* 优雅关闭，直到所剩数据发送完毕或超时 */
        optLinger.l_onoff = 1;
        optLinger.l_linger = 1;
    }
    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd_ < 0) {
        LOG_ERROR(MODULE_WEBSERVER, "Create socket error!", port_);
        return false;
    }

    ret = setsockopt(listenFd_, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(optLinger));
    if (ret < 0) {
        close(listenFd_);
        LOG_ERROR(MODULE_WEBSERVER, "Init linger error!", port_);
        return false;
    }

    int optval = 1;
    /* 端口复用 */
    /* 只有最后一个套接字会正常接收数据 */
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));
    if (ret == -1) {
        LOG_ERROR(MODULE_WEBSERVER, "set socket setsockopt error");
        close(listenFd_);
        return false;
    }

    ret = bind(listenFd_, (struct sockaddr*)&addr, sizeof(addr));
    if (ret < 0) {
        LOG_ERROR(MODULE_WEBSERVER, "Bind Port:%d error!", port_);
        close(listenFd_);
        return false;
    }

    ret = listen(listenFd_, 6);
    if (ret < 0) {
        LOG_ERROR(MODULE_WEBSERVER, "Listen port:%d error!", port_);
        close(listenFd_);
        return false;
    }
    ret = epoller_->AddFd(listenFd_, listenEvent_ | EPOLLIN);
    if (ret == 0) {
        LOG_ERROR(MODULE_WEBSERVER, "Add listen error!");
        close(listenFd_);
        return false;
    }
    SetFdNonblock(listenFd_);
    LOG_INFO(MODULE_WEBSERVER, "Server port:%d", port_);
    return true;
}

int WebServer::SetFdNonblock(int fd) {
    assert(fd > 0);
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
}

/**
 * 初始化SSL上下文
 * 
 * 设置SSL/TLS服务器环境，加载证书和私钥文件。
 * 这是启用HTTPS功能的关键步骤。
 * 
 * 初始化步骤：
 * 1. 初始化OpenSSL库
 * 2. 加载所有加密算法
 * 3. 加载错误字符串
 * 4. 创建SSL上下文
 * 5. 加载服务器证书
 * 6. 加载私钥文件
 * 7. 验证证书和私钥的匹配性
 */
void WebServer::InitSSL_() {
    // 初始化OpenSSL库，必须在其他SSL函数调用之前执行
    SSL_library_init();
    
    // 加载所有可用的加密算法（包括对称加密、非对称加密、哈希算法等）
    OpenSSL_add_all_algorithms();
    
    // 加载SSL错误字符串，便于调试和错误报告
    SSL_load_error_strings();
    
    // 创建SSL上下文，使用TLS服务器方法
    // TLS_server_method()会自动选择服务器支持的最高TLS版本
    sslCtx_ = SSL_CTX_new(TLS_server_method());
    if (!sslCtx_) {
        LOG_ERROR(MODULE_WEBSERVER, "Create SSL context failed");
        isClose_ = true;
        return;
    }
    
    // 加载服务器证书文件（PEM格式）
    if (SSL_CTX_use_certificate_file(sslCtx_, certPath_, SSL_FILETYPE_PEM) <= 0) {
        LOG_ERROR(MODULE_WEBSERVER, "Load certificate failed");
        ERR_print_errors_fp(stderr);  // 打印详细的SSL错误信息
        isClose_ = true;
        return;
    }
    
    // 加载私钥文件（PEM格式）
    if (SSL_CTX_use_PrivateKey_file(sslCtx_, keyPath_, SSL_FILETYPE_PEM) <= 0) {
        LOG_ERROR(MODULE_WEBSERVER, "Load private key failed");
        ERR_print_errors_fp(stderr);  // 打印详细的SSL错误信息
        isClose_ = true;
        return;
    }
    
    // 验证私钥是否与证书匹配
    if (!SSL_CTX_check_private_key(sslCtx_)) {
        LOG_ERROR(MODULE_WEBSERVER, "Private key does not match certificate");
        isClose_ = true;
    }
}

/**
 * 清理SSL资源
 * 
 * 在服务器关闭时清理SSL相关的资源，防止内存泄漏。
 * 包括释放SSL上下文和清理OpenSSL内部状态。
 */
void WebServer::CleanupSSL_() {
    if (sslCtx_) {
        // 释放SSL上下文，这会自动释放所有相关的SSL连接
        SSL_CTX_free(sslCtx_);
        sslCtx_ = nullptr;
    }
    
    // 清理OpenSSL的内部状态和内存
    // 注意：在较新版本的OpenSSL中，EVP_cleanup()已被弃用
    // 但为了兼容性，这里仍然保留
    EVP_cleanup();
}