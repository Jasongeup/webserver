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

WebServer::WebServer(int port, int trigMode, int timeoutMS, bool OptLinger,
              int sqlPort, const char* sqlUser, const char* sqlPwd,
              const char* dbName, int connPoolNum, int threadNum,
              bool openLog, int logLevel, int logQueSize):
              port_(port), openLinger_(OptLinger), timeoutMS_(timeoutMS), isClose_(false),
              timer_(new HeapTimer()), threadpool_(new ThreadPool(threadNum)), epoller_(new Epoller())
{
    srcDir_ = getcwd(nullptr, 256);
    assert(srcDir_);  // 获取当前工作目录的绝对路径
    strncat(srcDir_, "/resources/", 16);    // 附加到根目录末尾
    HttpConn::userCount = 0;        // 静态成员变量初始化
    HttpConn::srcDir = srcDir_;
    SqlConnPool::Instance()->Init("localhost", sqlPort, sqlUser, sqlPwd, dbName, connPoolNum); // 初始化数据库连接池

    InitEventMode_(trigMode);
    if (!InitSocket_()) {isClose_ = true;}

    if (openLog) {   // 记录参数的日志信息
        Log::Instance()->init(logLevel, "./log", ".log", logQueSize);  // 创建日志实例，并初始化
        if (isClose_) {LOG_ERROR("==========Server init error!==========");}
        else {
            LOG_INFO("==========Server init==========");  // 记录服务器初始化参数
            LOG_INFO("Port:%d, OpenLinger:%s", port_, OptLinger?"true":"false");
            LOG_INFO("Listen Mode:%s, OpenConn Mode:%s",
                    (listenEvent_ & EPOLLET ? "ET" : "LT"),
                    (connEvent_ & EPOLLET ? "ET" : "LT"));  // 记录事件触发模式
            LOG_INFO("LogSys level:%d", logLevel);
            LOG_INFO("srcDir:%s", HttpConn::srcDir);
            LOG_INFO("SqlConnPool num: %d, ThreadPool num: %d", connPoolNum, threadNum);
        }
    }
}

WebServer::~WebServer() {
    close(listenFd_);
    isClose_ = true;
    free(srcDir_);
    SqlConnPool::Instance()->ClosePool();
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
    if (!isClose_) {LOG_INFO("========== Server start ==========");}
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
                LOG_ERROR("Unexpected event");
            }
        }
    }
}

/* 给客户发送报错消息 */
void WebServer::SendError_(int fd, const char* info) {
    assert(fd > 0);
    int ret = send(fd, info, strlen(info), 0);
    if (ret < 0) {
        LOG_WARN("Send error to client[%d] error!", fd);
    }
    close(fd);
}

/* 关闭客户连接，删除epoll事件，关闭连接*/
void WebServer::CloseConn_(HttpConn* client) {  
    assert(client);
    LOG_INFO("Client[%d] quit!", client->GetFd());
    epoller_->DelFd(client->GetFd());
    client->Close();
}

/* 新连接到来，分配逻辑处理对象，注册读就绪事件 */
void WebServer::AddClient_(int fd, sockaddr_in addr) {
    assert(fd > 0);
    users_[fd].init(fd, addr);  // 给新连接socket分配处理逻辑对象
    if (timeoutMS_ > 0) {  // 给该连接分配定时器
        timer_->add(fd, timeoutMS_, std::bind(&WebServer::CloseConn_, this, &users_[fd]));
    }
    epoller_->AddFd(fd, EPOLLIN | connEvent_);   // 往epoll表中注册socket就绪事件
    SetFdNonblock(fd);
    LOG_INFO("Client[%d] in!", users_[fd].GetFd());
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
            LOG_WARN("Clients is full!");
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
        LOG_ERROR("Port:%d error", port_);
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
        LOG_ERROR("Create socket error!", port_);
        return false;
    }

    ret = setsockopt(listenFd_, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(optLinger));
    if (ret < 0) {
        close(listenFd_);
        LOG_ERROR("Init linger error!", port_);
        return false;
    }

    int optval = 1;
    /* 端口复用 */
    /* 只有最后一个套接字会正常接收数据 */
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));
    if (ret == -1) {
        LOG_ERROR("set socket setsockopt error");
        close(listenFd_);
        return false;
    }

    ret = bind(listenFd_, (struct sockaddr*)&addr, sizeof(addr));
    if (ret < 0) {
        LOG_ERROR("Bind Port:%d error!", port_);
        close(listenFd_);
        return false;
    }

    ret = listen(listenFd_, 6);
    if (ret < 0) {
        LOG_ERROR("Listen port:%d error!", port_);
        close(listenFd_);
        return false;
    }
    ret = epoller_->AddFd(listenFd_, listenEvent_ | EPOLLIN);
    if (ret == 0) {
        LOG_ERROR("Add listen error!");
        close(listenFd_);
        return false;
    }
    SetFdNonblock(listenFd_);
    LOG_INFO("Server port:%d", port_);
    return true;
}

int WebServer::SetFdNonblock(int fd) {
    assert(fd > 0);
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
}