/***********************************************
 * FileName    : httpconn.cpp
 * Description : see httpconn.h
 * 
 * Feature     :
 * 
 * Author      : JasonGe
 * Created on  : 2025/03/27
************************************************/
#include "httpConn.h"
using namespace std;

const char* HttpConn::srcDir;
std::atomic<int> HttpConn::userCount;  // 用户数量定义为原子类型，使改变该值的操作原子化
bool HttpConn::isET;


HttpConn::HttpConn() { 
    fd_ = -1;
    addr_ = { 0 };
    isClose_ = true;
};

HttpConn::~HttpConn() { 
    Close(); 
};

/* 根据与客户的连接socket，客户端的socket地址初始化对象 */
void HttpConn::init(int fd, const sockaddr_in& addr) {
    assert(fd > 0);
    userCount++;
    addr_ = addr;
    fd_ = fd;
    writeBuff_.RetrieveAll();
    readBuff_.RetrieveAll();
    isClose_ = false;
    LOG_INFO("Client[%d](%s:%d) in, userCount:%d", fd_, GetIP(), GetPort(), (int)userCount);
}

void HttpConn::Close() {
    response_.UnmapFile();
    if(isClose_ == false){
        isClose_ = true; 
        userCount--;
        close(fd_);
        LOG_INFO("Client[%d](%s:%d) quit, UserCount:%d", fd_, GetIP(), GetPort(), (int)userCount);
    }
}

int HttpConn::GetFd() const {
    return fd_;
};

struct sockaddr_in HttpConn::GetAddr() const {
    return addr_;
}

const char* HttpConn::GetIP() const {
    return inet_ntoa(addr_.sin_addr);
}

int HttpConn::GetPort() const {
    return addr_.sin_port;
}

/* 将连接socket上的数据读入缓冲区 */
ssize_t HttpConn::read(int* saveErrno) {
    ssize_t len = -1;
    do {
        len = readBuff_.ReadFd(fd_, saveErrno);
        if (len <= 0) {
            break;
        }
    } while (isET);  // ET模式下循环读用户数据
    return len;
}

/* 将响应数据写入连接socket */
ssize_t HttpConn::write(int* saveErrno) {
    ssize_t len = -1;
    do {
        len = writev(fd_, iov_, iovCnt_);  // 内存块集中写
        if(len <= 0) {
            *saveErrno = errno;
            break;
        }
        if(iov_[0].iov_len + iov_[1].iov_len  == 0) { break; } /* 传输结束 */
        else if(static_cast<size_t>(len) > iov_[0].iov_len) { // 第一块发送完，第二块没有
            iov_[1].iov_base = (uint8_t*) iov_[1].iov_base + (len - iov_[0].iov_len);
            iov_[1].iov_len -= (len - iov_[0].iov_len);
            if(iov_[0].iov_len) {
                writeBuff_.RetrieveAll();
                iov_[0].iov_len = 0;
            }
        }
        else {   // 第一块没发送完
            iov_[0].iov_base = (uint8_t*)iov_[0].iov_base + len; 
            iov_[0].iov_len -= len; 
            writeBuff_.Retrieve(len);
        }
    } while(isET || ToWriteBytes() > 10240);  // ET模式下循环写
    return len;
}

/* 分析请求，并写应答报文到写缓冲区，返回是否发送给客户的所有数据准备就绪 */
bool HttpConn::process() {
    request_.Init();
    if(readBuff_.ReadableBytes() <= 0) { 
        return false;
    }
    else if(request_.parse(readBuff_)) {  // 调用成员类的方法读数据
        LOG_DEBUG("%s", request_.path().c_str());
        response_.Init(srcDir, request_.path(), request_.IsKeepAlive(), 200); //初始化相应数据类成员
    } else {  // 如果读失败？
        response_.Init(srcDir, request_.path(), false, 400);
    }

    response_.MakeResponse(writeBuff_);
    /* 第一个内存块指向响应头 */
    iov_[0].iov_base = const_cast<char*>(writeBuff_.Peek());
    iov_[0].iov_len = writeBuff_.ReadableBytes();
    iovCnt_ = 1;

    /* 第二个内存块指向文件 */
    if(response_.FileLen() > 0  && response_.File()) {
        iov_[1].iov_base = response_.File();  // 指向用户请求的文件在内存的映射
        iov_[1].iov_len = response_.FileLen();
        iovCnt_ = 2;
    }
    LOG_DEBUG("filesize:%d, %d  to %d", response_.FileLen() , iovCnt_, ToWriteBytes());
    return true;
}