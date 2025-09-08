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
/* SSL写入块大小常量
 * 由于SSL/TLS协议的特性，建议将大块数据分块发送以提高性能和稳定性
 * 16KB是一个经验值，平衡了性能和内存使用
 */
static const size_t SSL_CHUNK = 16 * 1024; // 每次用 SSL_write 发送的最大字节数


HttpConn::HttpConn() { 
    fd_ = -1;
    addr_ = { 0 };
    isClose_ = true;
    ssl_ = nullptr;  // 初始化SSL指针，表示默认使用普通HTTP连接
};

HttpConn::~HttpConn() { 
    Close(); 
};

/* 初始化HTTP连接对象，支持SSL/TLS加密通信
 * @param fd socket文件描述符
 * @param addr 客户端地址信息
 * @param ssl SSL会话对象，用于HTTPS加密通信，nullptr表示普通HTTP连接
 */
void HttpConn::init(int fd, const sockaddr_in& addr, SSL* ssl) {
    assert(fd > 0);
    userCount++;
    addr_ = addr;
    fd_ = fd;
    ssl_ = ssl;  // 设置SSL对象，用于后续的加密数据传输
    writeBuff_.RetrieveAll(); // writeBuff_通过Buffer类的默认构造函数隐式初始化了
    readBuff_.RetrieveAll();
    isClose_ = false;
    LOG_INFO(MODULE_HTTP, "Client[%d](%s:%d) in, userCount:%d", fd_, GetIP(), GetPort(), (int)userCount);
}

void HttpConn::Close() {
    response_.UnmapFile();
    if(isClose_ == false){
        isClose_ = true; 
        userCount--;
        /* 如果使用SSL连接，需要优雅关闭SSL会话 */
        if (ssl_) {
            SSL_shutdown(ssl_);  // 发送SSL关闭通知，优雅关闭加密通道
            SSL_free(ssl_);      // 释放SSL会话对象及其相关资源
            ssl_ = nullptr;      // 将指针置空，防止重复释放
        }
        close(fd_);
        LOG_INFO(MODULE_HTTP, "Client[%d](%s:%d) quit, UserCount:%d", fd_, GetIP(), GetPort(), (int)userCount);
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

/* 从连接socket读取数据到缓冲区，支持SSL/TLS加密通信
 * @param saveErrno 保存错误码的指针
 * @return 读取的字节数，-1表示需要重试，-2表示错误
 */
ssize_t HttpConn::read(int* saveErrno) {
    ssize_t len = -1;
    do {
        if (ssl_) {
            len = readBuff_.ReadFdSSL(ssl_, saveErrno);  // 使用SSL加密读取，数据会自动解密
        } else {
            len = readBuff_.ReadFd(fd_, saveErrno);      // 普通socket读取，数据未加密
        }
        if (len <= 0) {
            break;
        }
    } while (isET);  // ET模式下循环读用户数据
    return len;
}

/* SSL分块写入函数，确保所有数据都被正确发送
 * SSL/TLS协议要求数据分块发送，此函数处理大块数据的分块传输
 * @param ssl SSL会话对象
 * @param buf 要发送的数据缓冲区
 * @param len 数据长度
 * @return 已写入字节数，-1表示需要重试(EAGAIN)，-2表示错误
 */
ssize_t ssl_write_all(SSL* ssl, const char* buf, size_t len) {
    size_t written = 0;
    while (written < len) {
        // 分块发送，每次最多发送SSL_CHUNK字节
        int n = SSL_write(ssl, buf + written, (int)std::min<size_t>(len - written, SSL_CHUNK));
        if (n > 0) {
            written += (size_t)n;
            continue;
        }
        // 检查SSL错误类型
        int ssl_err = SSL_get_error(ssl, n);
        if (ssl_err == SSL_ERROR_WANT_WRITE || ssl_err == SSL_ERROR_WANT_READ) {
            errno = EAGAIN; // 非阻塞模式，需要等待可写/可读后重试
            return -1;
        } else {
            // SSL致命错误，打印详细错误信息
            ERR_print_errors_fp(stderr);
            errno = EIO;
            return -2;
        }
    }
    return (ssize_t)written;
}

/* 将缓冲区数据写入socket，支持SSL/TLS加密通信
 * @param saveErrno 保存错误码的指针
 * @return 写入的字节数，-1表示需要重试，-2表示错误
 */
ssize_t HttpConn::write(int* saveErrno) {
    ssize_t len = -1;
    do {
        if (ssl_) {
            /* SSL加密写入分支 - 需要分别发送响应头和文件内容
             * SSL/TLS协议要求数据分块发送，这里分别处理HTTP响应头和文件数据
             */
            // 先发送 iov_[0]（HTTP响应头/头+部分 body，如果 writeBuff 包含了全部头和可能的小 body）
            if (iovCnt_ >= 1 && iov_[0].iov_len > 0) {
                ssize_t n = ssl_write_all(ssl_, (const char*)iov_[0].iov_base, iov_[0].iov_len);
                if (n == -1) {
                    // 可重试（非阻塞），上层应等待可写（errno 已是 EAGAIN）
                    *saveErrno = errno;
                    return -1;
                } else if (n == -2) {
                    *saveErrno = errno;
                    return -1;
                } else {
                    // 全部写完或部分写完（ssl_write_all 会保证全部或返回上面错误），
                    // 清空 writeBuff_ 对应已写内容
                    writeBuff_.Retrieve((size_t)n); // 如果 n 等于原 iov_[0].iov_len，会把头取完
                    // 将 iov_[0] 指向剩余的数据（通常已被取完）
                    iov_[0].iov_base = (char*)iov_[0].iov_base + n;
                    iov_[0].iov_len -= n;
                }
            }

            /* 发送文件内容（如果存在）
             * 对于大文件，采用分块发送方式，避免内存占用过大
             */
            if (iovCnt_ >= 2 && iov_[1].iov_len > 0) {
                char* filePtr = (char*)iov_[1].iov_base;
                size_t remaining = iov_[1].iov_len;
                // 采用分块方式发送大文件，每次最多发送SSL_CHUNK字节
                while (remaining > 0) {
                    size_t chunk = std::min<size_t>(remaining, SSL_CHUNK);
                    ssize_t wn = ssl_write_all(ssl_, filePtr, chunk);
                    if (wn == -1) {
                        // EAGAIN，可重试（上层应等待写就绪再调用 write()）
                        *saveErrno = errno;
                        // 更新 iov_[1] 的偏移，以便下次继续发送
                        iov_[1].iov_base = filePtr;
                        iov_[1].iov_len = remaining;
                        return -1;
                    } else if (wn == -2) {
                        *saveErrno = errno;
                        return -1;
                    } else {
                        // 成功发送了 wn 字节，更新指针和剩余长度
                        filePtr += wn;
                        remaining -= (size_t)wn;
                        // 同步更新映射/长度（如果把整个文件发送完，要调用 response_.UnmapFile() elsewhere）
                    }
                }
                // 全部文件写完
                writeBuff_.RetrieveAll(); // 如果习惯把文件发送视为最后一步，可清空缓冲
                iov_[1].iov_base = nullptr;
                iov_[1].iov_len = 0;
            }

            // 如果到此处都成功（写完所有数据），返回最后一次写的字节数（示例返回 0 表示无错误）
            // 但上层的逻辑可能希望 len 表示最后一次写返回值，设置为 1 表示成功
            len = 1;
        } else {
            /* 普通HTTP写入（非SSL）
             * 使用writev系统调用一次性发送多个缓冲区
             */
            len = writev(fd_, iov_, iovCnt_);
            if(len <= 0) {
                *saveErrno = errno;
                break;
            }
            // 处理部分写入的情况
            if(static_cast<size_t>(len) > iov_[0].iov_len) {
                // 第一个缓冲区已全部写入，更新第二个缓冲区
                iov_[1].iov_base = (uint8_t*) iov_[1].iov_base + (len - iov_[0].iov_len);
                iov_[1].iov_len -= (len - iov_[0].iov_len);
                if(iov_[0].iov_len) {
                    writeBuff_.RetrieveAll();
                    iov_[0].iov_len = 0;
                }
            }
            else {
                // 只写入了第一个缓冲区的一部分
                iov_[0].iov_base = (uint8_t*)iov_[0].iov_base + len; 
                iov_[0].iov_len -= len; 
                writeBuff_.Retrieve(len);
            }
        }
    } while (isET && ToWriteBytes() > 0);
    return len;
}

/* 分析请求，并写应答报文到写缓冲区，返回是否发送给客户的所有数据准备就绪 */
bool HttpConn::process() {
    request_.Init();
    if(readBuff_.ReadableBytes() <= 0) { 
        return false;
    }
    else if(request_.parse(readBuff_)) {  // 调用成员类的方法读数据
        LOG_DEBUG(MODULE_HTTP, "%s", request_.path().c_str());
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
    LOG_DEBUG(MODULE_HTTP, "filesize:%d, %d  to %d", response_.FileLen() , iovCnt_, ToWriteBytes());
    return true;
}