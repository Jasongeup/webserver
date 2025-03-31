/***********************************************************
 * FileName    : webserver.h
 * Description : This header file define a class named WebServer, 
 *               which organize all parts of the server.
 * 
 * Features    : 
 *    - accept socket and manage corresponding task deal unit
 *    - write log file  
 *    - manage connection timeout
 * 
 * Author      : JasonGe
 * Created on  : 2025/03/26
 * 
***********************************************************/
#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <unordered_map>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "epoller.h"
#include "../logsys/log.h"
#include "../timer/heapTimer.h"
#include "../pool/sqlconnpool.h"
#include "../pool/threadpool.h"
#include "../pool/sqlconnRAII.h"
#include "../httpConnection/httpconn.h"

class WebServer {
public:
    /* WebServer的初始化参数，包括：
        端口，触发模式(ET/LT)，定时时间timeoutMs, 优雅关闭连接，
        数据库端口，数据库用户名，数据库密码，
        数据库名称，连接池的数量，线程池数量
        日志开关，日志等级，日志异步队列容量
    */
    WebServer(int port, int trigMode, int timeoutMs, bool OptLinger,
              int sqlPort, const char* sqlUser, const char* sqlPwd,
              const char* dbName, int connPoolNum, int threadNum,
              bool openLog, int logLevel, int logQueSize);
    
    ~WebServer();
    void Start();

private:
    bool InitSocket_();
    void InitEventMode_(int trigMode);
    void AddClient_(int fd, sockaddr_in addr);

    void DealListen_();
    void DealWrite_(HttpConn* client);
    void DealRead_(HttpConn* client);

    void SendError_(int fd, const char* info);
    void ExtentTime_(HttpConn* client);
    void CloseConn_(HttpConn* client);

    void OnRead_(HttpConn* client);
    void OnWrite_(HttpConn* client);
    void OnProcess(HttpConn* client);

    static const int MAX_FD = 65536;

    static int SetFdNonblock(int fd);

    int port_;      // 服务器端口
    bool openLinger_;
    int timeoutMS_;  // 毫秒
    bool isClose_;   // 服务器开关
    int listenFd_;   // 监听socket
    char* srcDir_;   // 服务器根目录

    uint32_t listenEvent_;   // 监听socket的epoll事件，是否设置ET模式
    uint32_t connEvent_;     // 连接socket的epoll事件,是否设置ET模式

    std::unique_ptr<HeapTimer> timer_;    // 定时器
    std::unique_ptr<ThreadPool> threadpool_;   // 线程池
    std::unique_ptr<Epoller> epoller_;    // epoll表
    std::unordered_map<int, HttpConn> users_;    // key是连接socket，value是逻辑处理单元
};


#endif  // WEBSERVER_H