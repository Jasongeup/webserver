/***********************************************************
 * FileName    : httpConn.h
 * Description : This header file define a class named httpconn, 
 *               which deal the read/write task of connected-socket.
 * 
 * Features    : 
 *   - read data from socket and send respose to socket
 *   - parse client-request and return required file
 * 
 * Author      : JasonGe
 * Created on  : 2025/03/27
 * 
***********************************************************/

 #ifndef HTTP_CONN_H
 #define HTTP_CONN_H
 
 #include <sys/types.h>
 #include <sys/uio.h>     // readv/writev
 #include <arpa/inet.h>   // sockaddr_in
 #include <stdlib.h>      // atoi()
 #include <errno.h>      
 
 #include "../log/log.h"
 #include "../pool/sqlconnRAII.h"
 #include "../buffer/buffer.h"
 #include "httpRequest.h"
 #include "httpResponse.h"
 
 class HttpConn {
 public:
     HttpConn();
 
     ~HttpConn();
 
     void init(int sockFd, const sockaddr_in& addr);
 
     ssize_t read(int* saveErrno);
 
     ssize_t write(int* saveErrno);
 
     void Close();
 
     int GetFd() const;
 
     int GetPort() const;
 
     const char* GetIP() const;
     
     sockaddr_in GetAddr() const;
     
     bool process();
 
     int ToWriteBytes() { 
         return iov_[0].iov_len + iov_[1].iov_len; 
     }
 
     bool IsKeepAlive() const {
         return request_.IsKeepAlive();
     }
 
     static bool isET;
     static const char* srcDir;
     static std::atomic<int> userCount;
     
 private:
    
     int fd_;
     struct  sockaddr_in addr_;
 
     bool isClose_;
     
     int iovCnt_;  // 有数据的块的数量
     struct iovec iov_[2];  // 集中写内存块，用于发送数据
     
     Buffer readBuff_; // 读缓冲区
     Buffer writeBuff_; // 写缓冲区
 
     HttpRequest request_;  // 解析客户请求  
     HttpResponse response_;  // 响应客户请求
 };
 
 
 #endif //HTTP_CONN_H