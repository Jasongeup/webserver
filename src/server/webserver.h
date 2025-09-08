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
#include "../pool/sqlConnPool.h"
#include "../pool/threadPool.h"
#include "../pool/sqlConnRAII.h"
#include "../httpConnection/httpConn.h"

class WebServer {
public:
    /* WebServer的初始化参数，包括：
        端口，触发模式(ET/LT)，定时时间timeoutMs, 优雅关闭连接，
        数据库端口，数据库用户名，数据库密码，
        数据库名称，连接池的数量，线程池数量
        日志开关，日志等级，日志异步队列容量
    */
    /* WebServer构造函数，支持SSL/TLS配置
     * @param useSSL 是否启用SSL/TLS加密通信
     * @param certPath SSL证书文件路径(.pem格式)，包含服务器公钥和CA签名
     * @param keyPath SSL私钥文件路径(.pem格式)，用于解密和数字签名
     */
    WebServer(int port, int trigMode, int timeoutMS, bool OptLinger,
              int sqlPort, const char* sqlUser, const char* sqlPwd,
              const char* dbName, int connPoolNum, int threadNum,
              bool openLog, int logLevel, int logQueSize,
              bool useSSL, const char* certPath, const char* keyPath);
    
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

    /* SSL/TLS相关方法 */
    void InitSSL_();        // 初始化SSL上下文，加载证书和私钥
    void CleanupSSL_();     // 清理SSL资源，释放SSL上下文

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
    
    /* SSL/TLS相关成员变量 */
    bool useSSL_;        // 是否启用SSL/TLS加密通信，true表示支持HTTPS
    const char* certPath_;   // SSL证书文件路径，包含服务器公钥和CA签名信息
    const char* keyPath_;    // SSL私钥文件路径，用于解密和数字签名
    SSL_CTX* sslCtx_;       // SSL上下文对象，管理SSL连接配置和证书信息
};


#endif  // WEBSERVER_H