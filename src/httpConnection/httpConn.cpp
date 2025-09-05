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
#include "../websocket_handler.h"
#include <cstdio>
using namespace std;

// 执行外部命令
std::string exec(const char* cmd) {
    char buffer[128];
    std::string result = "";
    FILE* pipe = popen(cmd, "r");
    if (pipe == nullptr) {
        return "ERROR: Failed to open pipe";
    }
    
    while (!feof(pipe)) {
        if (fgets(buffer, 128, pipe) != NULL)
            result += buffer;
    }
    
    int status = pclose(pipe);
    if (status != 0) {
        result = "ERROR: Command failed with status " + std::to_string(status);
    }
    
    return result;
}

const char* HttpConn::srcDir;
std::atomic<int> HttpConn::userCount;  // 用户数量定义为原子类型，使改变该值的操作原子化
bool HttpConn::isET;
static const size_t SSL_CHUNK = 16 * 1024; // 每次用 SSL_write 发送的最大字节数


HttpConn::HttpConn() { 
    fd_ = -1;
    addr_ = { 0 };
    isClose_ = true;
    ssl_ = nullptr;  // 初始化SSL指针
};

HttpConn::~HttpConn() { 
    Close(); 
};

/* 根据与客户的连接socket，客户端的socket地址初始化对象 */
void HttpConn::init(int fd, const sockaddr_in& addr, SSL* ssl) {  // 添加SSL参数
    assert(fd > 0);
    userCount++;
    addr_ = addr;
    fd_ = fd;
    ssl_ = ssl;  // 设置SSL对象
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
        if (ssl_) {
            SSL_shutdown(ssl_);  // 优雅关闭SSL连接
            SSL_free(ssl_);
            ssl_ = nullptr;
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

/* 将连接socket上的数据读入缓冲区 */
ssize_t HttpConn::read(int* saveErrno) {
    ssize_t len = -1;
    do {
        if (ssl_) {
            len = readBuff_.ReadFdSSL(ssl_, saveErrno);  // 使用SSL读取
        } else {
            len = readBuff_.ReadFd(fd_, saveErrno);  // 普通读取
        }
        if (len <= 0) {
            break;
        }
    } while (isET);  // ET模式下循环读用户数据
    return len;
}

// 返回已写字节数，出现可重试情况时返回 -1 并设置 errno = EAGAIN，出现真正错误返回 -2
ssize_t ssl_write_all(SSL* ssl, const char* buf, size_t len) {
    size_t written = 0;
    while (written < len) {
        int n = SSL_write(ssl, buf + written, (int)std::min<size_t>(len - written, SSL_CHUNK));
        if (n > 0) {
            written += (size_t)n;
            continue;
        }
        int ssl_err = SSL_get_error(ssl, n);
        if (ssl_err == SSL_ERROR_WANT_WRITE || ssl_err == SSL_ERROR_WANT_READ) {
            errno = EAGAIN; // 上层事件循环应等待可写/可读后重试
            return -1;
        } else {
            // 真正错误
            ERR_print_errors_fp(stderr);
            errno = EIO;
            return -2;
        }
    }
    return (ssize_t)written;
}

/* 将缓冲区数据写入socket */
ssize_t HttpConn::write(int* saveErrno) {
    ssize_t len = -1;
    do {
        if (ssl_) {
            // SSL 分支 - 需要分别发送 header(iov_[0]) 和 file(iov_[1])（若存在）
            // 先发送 iov_[0]（响应头/头+部分 body，如果 writeBuff 包含了全部头和可能的小 body）
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

            // 若存在文件需要发送
            if (iovCnt_ >= 2 && iov_[1].iov_len > 0) {
                char* filePtr = (char*)iov_[1].iov_base;
                size_t remaining = iov_[1].iov_len;
                // 采用分块方式发送大文件
                while (remaining > 0) {
                    size_t chunk = std::min<size_t>(remaining, SSL_CHUNK);
                    ssize_t wn = ssl_write_all(ssl_, filePtr, chunk);
                    if (wn == -1) {
                        // EAGAIN，可重试（上层应等待写就绪再调用 write()）
                        *saveErrno = errno;
                        // 更新 iov_[1] 的偏移，以便下次继续
                        iov_[1].iov_base = filePtr;
                        iov_[1].iov_len = remaining;
                        return -1;
                    } else if (wn == -2) {
                        *saveErrno = errno;
                        return -1;
                    } else {
                        // 写了 wn 字节
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
            // 普通HTTP写入
            len = writev(fd_, iov_, iovCnt_);
            if(len <= 0) {
                *saveErrno = errno;
                break;
            }
            if(static_cast<size_t>(len) > iov_[0].iov_len) {
                iov_[1].iov_base = (uint8_t*) iov_[1].iov_base + (len - iov_[0].iov_len);
                iov_[1].iov_len -= (len - iov_[0].iov_len);
                if(iov_[0].iov_len) {
                    writeBuff_.RetrieveAll();
                    iov_[0].iov_len = 0;
                }
            }
            else {
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
        LOG_INFO(MODULE_HTTP, "Processing path: %s", request_.path().c_str());
        LOG_INFO(MODULE_HTTP, "Request method: %s", request_.method().c_str());
        LOG_INFO(MODULE_HTTP, "Request body: %s", request_.body().c_str());
        
        if(request_.path() == "/chat") {
            LOG_INFO(MODULE_HTTP, "Chat endpoint accessed");
            // 处理聊天请求
            std::string message = request_.GetPost("message");
            LOG_INFO(MODULE_HTTP, "GetPost message: '%s'", message.c_str());
            
            if(message.empty()) {
                // 尝试从JSON中解析
                std::string body = request_.body();
                LOG_INFO(MODULE_HTTP, "Request body: '%s'", body.c_str());
                // 使用更简单的JSON解析
                size_t start = body.find("message");
                LOG_INFO(MODULE_HTTP, "Looking for 'message' in body, start position: %zu", start);
                if (start != std::string::npos) {
                    // 找到 "message" 后，寻找冒号和引号
                    size_t colon = body.find(':', start);
                    if (colon != std::string::npos) {
                        size_t quote1 = body.find('"', colon);
                        if (quote1 != std::string::npos) {
                            size_t quote2 = body.find('"', quote1 + 1);
                            if (quote2 != std::string::npos) {
                                message = body.substr(quote1 + 1, quote2 - quote1 - 1);
                                LOG_INFO(MODULE_HTTP, "Extracted message: '%s'", message.c_str());
                            }
                        }
                    }
                } else {
                    LOG_INFO(MODULE_HTTP, "Pattern 'message' not found in body");
                }
            }
            
            if(message.empty()) {
                LOG_INFO(MODULE_HTTP, "No message provided, returning error");
                response_.SetContent("{\"error\": \"No message provided\"}", "application/json");
                response_.Init(srcDir, request_.path(), request_.IsKeepAlive(), 400);
            } else {
                LOG_INFO(MODULE_HTTP, "Processing message: '%s'", message.c_str());
                
                // 调用真正的LLM API
                LOG_INFO(MODULE_HTTP, "Calling LLM API via HTTP");
                
                // 构建HTTP请求到LLM API，设置较长的超时时间以支持真正的LLM推理
                std::string curl_cmd = "curl -s -X POST http://localhost:8000/chat "
                                     "-H \"Content-Type: application/json\" "
                                     "-d '{\"message\": \"" + message + "\"}' "
                                     "--max-time 30 --connect-timeout 5";
                
                LOG_INFO(MODULE_HTTP, "Executing curl command: %s", curl_cmd.c_str());
                std::string response = exec(curl_cmd.c_str());
                LOG_INFO(MODULE_HTTP, "LLM response: '%s'", response.c_str());
                
                std::string final_response;
                // 检查响应是否有效
                if(response.empty() || response.find("ERROR") != std::string::npos || response.find("error") != std::string::npos) {
                    LOG_INFO(MODULE_HTTP, "LLM service error, using fallback response");
                    // 如果LLM API失败，使用智能回退响应
                    final_response = "{\"response\": \"抱歉，AI服务暂时不可用。不过我可以告诉你，你刚才问的是：'" + message + "'。请稍后再试或联系管理员。\"}";
                } else {
                    LOG_INFO(MODULE_HTTP, "LLM service success, processing response");
                    // 确保响应是有效的JSON格式
                    if(response.find("{") == std::string::npos) {
                        final_response = "{\"response\": \"" + response + "\"}";
                    } else {
                        final_response = response;
                    }
                }
                
                LOG_INFO(MODULE_HTTP, "Final response: %s", final_response.c_str());
                
                // 直接构建HTTP响应，不通过文件系统
                std::string http_response = "HTTP/1.1 200 OK\r\n"
                                          "Content-Type: application/json\r\n"
                                          "Content-Length: " + std::to_string(final_response.length()) + "\r\n"
                                          "Connection: " + (request_.IsKeepAlive() ? "keep-alive" : "close") + "\r\n\r\n"
                                          + final_response;
                
                writeBuff_.Append(http_response);
                iov_[0].iov_base = const_cast<char*>(writeBuff_.Peek());
                iov_[0].iov_len = writeBuff_.ReadableBytes();
                iovCnt_ = 1;
                LOG_INFO(MODULE_HTTP, "Response set successfully, length: %zu", writeBuff_.ReadableBytes());
                LOG_INFO(MODULE_HTTP, "Final response: %s", final_response.c_str());
                return true; // 直接返回，不执行后续的MakeResponse
            }
        } else {
            response_.Init(srcDir, request_.path(), request_.IsKeepAlive(), 200); //初始化相应数据类成员
        }
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