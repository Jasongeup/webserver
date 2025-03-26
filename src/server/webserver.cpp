/***********************************************
 * FileName    : webserver.cpp
 * Description : 
 * 
 * Feature     :
 * 
 * Author      : JasonGe
 * Created on  : 2025/03/26
************************************************/

#include "webserver.h"

WebServer::WebServer(int port, int trigMode, int timeoutMs, bool OptLinger,
              int sqlPort, const char* sqlUser, const char* sqlPwd,
              const char* dbName, int connPoolNum, int threadNum,
              bool openLog, int logLevel, int logQueSize):
              port_(port), openLinger_(OptLinger), timeoutMS_(timeoutMs), isClose_(false),
              timer_(new HeapTimer()), threadpool_(new ThreadPool(threadNum)), epoller_(new Epoller())
{
    srcDir_ = new char[256];
    assert(getcwd(srcDir_, 256));  // 获取当前工作目录的绝对路径
    strncat(srcDir_, "/resources/", 16);    // 附加到根目录末尾
    HttpConn::userCount = 0;        // 静态变量初始化？
    HttpConn::srcDir = srcDir_;
    SqlConnPool::Instance()->Init("localhost", sqlPort, sqlUser, sqlPwd, dbName, connPoolNum);

    InitEventMode_(trigMode);
    if (!InitSocket_()) {isClose_ = true;}

    if (openLog) {   // 记录参数的日志信息
        Log::Instance()->init(logLevel, "./log", ".log", logQueSize);
        if (isClose_) {LOG_ERROR("==========Server init error!==========");}
        else {
            LOG_INFO("==========Server init==========");
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
    delete[] srcDir_;
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
    int timeMs = -1;  /* epoll wait timeout == -1 无事件将阻塞 */
    if (!isClose_) {LOG_INFO("========== Server start ==========");}
    while (!isClose) {
        if (timeoutMS > 0) {
            timeMs = timer_->GetNextTick();
        }
        int eventCnt = epoller_->Wait(timeMs);  // 等待就绪事件epoll_wait
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
            else if (events & EPOLLUOT) {  // 如果是写就绪事件，断言对应任务存在，并执行写socket操作
                assert(users_.count(fd) > 0);
                DealWrite_(users_[fd]);
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
    epoller->DelFd(client->GetFD());
    client->Close();
}

/* 新连接到来，分配逻辑处理对象，注册读就绪事件 */
void WebServer::AddClient_(int fd, sockaddr_in addr) {
    assert(fd > 0);
    users_[fd].init(fd, addr);  // 给新连接socket分配处理逻辑对象
    if (timeoutMS_ > 0) {
        timer_->add(fd, timeoutMS_, std::bind(&WebServer::CloseConn_, this, &user_[fd]));
    }
    epoller_->AddFd(fd, EPOLLIN | connEvent_);   // 往epoll表中注册socket就绪事件
    SetFdNonblock(fd);
    LOG_INFO("Client[%d] in!", users_->GetFd());
}