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
#include <cstdio>
using namespace std;

/**
 * 执行外部命令
 * 
 * 通过popen执行系统命令并获取输出结果。
 * 主要用于调用curl命令与LLM API进行通信。
 * 
 * @param cmd 要执行的命令字符串
 * @return 命令的输出结果，失败时返回错误信息
 */
std::string exec(const char* cmd) {
    char buffer[128];
    std::string result = "";
    
    // 使用popen执行命令并获取输出流
    FILE* pipe = popen(cmd, "r");
    if (pipe == nullptr) {
        return "ERROR: Failed to open pipe";
    }
    
    // 逐行读取命令输出
    while (!feof(pipe)) {
        if (fgets(buffer, 128, pipe) != NULL)
            result += buffer;
    }
    
    // 关闭管道并获取命令退出状态
    int status = pclose(pipe);
    if (status != 0) {
        result = "ERROR: Command failed with status " + std::to_string(status);
    }
    
    return result;
}

// 静态成员变量定义
const char* HttpConn::srcDir;
std::atomic<int> HttpConn::userCount;  // 用户数量定义为原子类型，使改变该值的操作原子化
bool HttpConn::isET;
static const size_t SSL_CHUNK = 16 * 1024; // SSL写入时的分块大小，避免单次写入过多数据

/**
 * HttpConn构造函数
 * 
 * 初始化HTTP连接对象，设置默认值
 */
HttpConn::HttpConn() { 
    fd_ = -1;           // 文件描述符初始化为无效值
    addr_ = { 0 };      // 地址结构体清零
    isClose_ = true;    // 初始状态为关闭
    ssl_ = nullptr;     // SSL对象初始化为空指针
};

/**
 * HttpConn析构函数
 * 
 * 确保连接被正确关闭，释放资源
 */
HttpConn::~HttpConn() { 
    Close(); 
};

/**
 * 初始化HTTP连接对象
 * 
 * 设置连接的基本信息，包括socket、地址和SSL对象
 * 
 * @param fd socket文件描述符
 * @param addr 客户端地址信息
 * @param ssl SSL连接对象（如果启用SSL）
 */
void HttpConn::init(int fd, const sockaddr_in& addr, SSL* ssl) {
    assert(fd > 0);
    userCount++;        // 原子操作增加用户计数
    addr_ = addr;       // 保存客户端地址
    fd_ = fd;           // 保存文件描述符
    ssl_ = ssl;         // 设置SSL对象（可能为nullptr）
    
    // 清空读写缓冲区
    writeBuff_.RetrieveAll();
    readBuff_.RetrieveAll();
    
    isClose_ = false;   // 标记连接为打开状态
    LOG_INFO(MODULE_HTTP, "Client[%d](%s:%d) in, userCount:%d", fd_, GetIP(), GetPort(), (int)userCount);
}

/**
 * 关闭HTTP连接
 * 
 * 优雅关闭连接，包括SSL连接的清理和资源释放
 */
void HttpConn::Close() {
    response_.UnmapFile();  // 取消内存映射文件
    if(isClose_ == false){
        isClose_ = true; 
        userCount--;        // 原子操作减少用户计数
        
        // 如果使用SSL，需要优雅关闭SSL连接
        if (ssl_) {
            SSL_shutdown(ssl_);  // 发送SSL关闭通知给客户端
            SSL_free(ssl_);      // 释放SSL对象
            ssl_ = nullptr;      // 清空SSL指针
        }
        
        close(fd_);  // 关闭socket文件描述符
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

/**
 * 从连接socket读取数据到缓冲区
 * 
 * 根据是否启用SSL选择相应的读取方法。
 * 在ET模式下会循环读取直到没有更多数据。
 * 
 * @param saveErrno 用于保存错误码的指针
 * @return 读取的字节数，-1表示需要重试，-2表示错误
 */
ssize_t HttpConn::read(int* saveErrno) {
    ssize_t len = -1;
    do {
        if (ssl_) {
            // 使用SSL读取，数据会被自动解密
            len = readBuff_.ReadFdSSL(ssl_, saveErrno);
        } else {
            // 普通socket读取
            len = readBuff_.ReadFd(fd_, saveErrno);
        }
        if (len <= 0) {
            break;  // 读取完成或出错
        }
    } while (isET);  // ET模式下循环读取，直到没有更多数据
    return len;
}

/**
 * SSL安全写入函数
 * 
 * 确保所有数据都被写入SSL连接，处理SSL特有的错误情况。
 * 使用分块写入避免单次写入过多数据。
 * 
 * @param ssl SSL连接对象
 * @param buf 要写入的数据缓冲区
 * @param len 数据长度
 * @return 已写入的字节数，-1表示需要重试，-2表示致命错误
 */
ssize_t ssl_write_all(SSL* ssl, const char* buf, size_t len) {
    size_t written = 0;
    
    // 循环写入直到所有数据都被发送
    while (written < len) {
        // 分块写入，避免单次写入过多数据
        int n = SSL_write(ssl, buf + written, (int)std::min<size_t>(len - written, SSL_CHUNK));
        
        if (n > 0) {
            written += (size_t)n;  // 更新已写入字节数
            continue;              // 继续写入剩余数据
        }
        
        // 处理SSL写入错误
        int ssl_err = SSL_get_error(ssl, n);
        if (ssl_err == SSL_ERROR_WANT_WRITE || ssl_err == SSL_ERROR_WANT_READ) {
            // 非致命错误：需要等待socket可写或可读
            errno = EAGAIN;
            return -1;
        } else {
            // 致命错误：SSL连接出现问题
            ERR_print_errors_fp(stderr);  // 打印详细错误信息
            errno = EIO;
            return -2;
        }
    }
    
    return (ssize_t)written;  // 返回实际写入的字节数
}

/**
 * 将缓冲区数据写入socket
 * 
 * 根据是否启用SSL选择相应的写入方法。
 * SSL模式下需要分块写入，普通模式下使用writev进行分散写入。
 * 
 * @param saveErrno 用于保存错误码的指针
 * @return 写入的字节数，-1表示需要重试，-2表示错误
 */
ssize_t HttpConn::write(int* saveErrno) {
    ssize_t len = -1;
    do {
        if (ssl_) {
            // SSL模式：需要分别发送HTTP头和文件内容
            // 先发送HTTP响应头（iov_[0]）
            if (iovCnt_ >= 1 && iov_[0].iov_len > 0) {
                ssize_t n = ssl_write_all(ssl_, (const char*)iov_[0].iov_base, iov_[0].iov_len);
                if (n == -1) {
                    // 可重试（非阻塞），上层应等待可写
                    *saveErrno = errno;
                    return -1;
                } else if (n == -2) {
                    // 致命错误
                    *saveErrno = errno;
                    return -1;
                } else {
                    // 成功写入，更新缓冲区状态
                    writeBuff_.Retrieve((size_t)n);  // 从写缓冲区移除已发送的数据
                    iov_[0].iov_base = (char*)iov_[0].iov_base + n;  // 更新iovec指针
                    iov_[0].iov_len -= n;  // 更新剩余长度
                }
            }

            // 如果存在文件需要发送（iov_[1]）
            if (iovCnt_ >= 2 && iov_[1].iov_len > 0) {
                char* filePtr = (char*)iov_[1].iov_base;
                size_t remaining = iov_[1].iov_len;
                
                // 采用分块方式发送大文件，避免单次发送过多数据
                while (remaining > 0) {
                    size_t chunk = std::min<size_t>(remaining, SSL_CHUNK);
                    ssize_t wn = ssl_write_all(ssl_, filePtr, chunk);
                    if (wn == -1) {
                        // EAGAIN，可重试（上层应等待写就绪再调用write()）
                        *saveErrno = errno;
                        // 更新iov_[1]的偏移，以便下次继续
                        iov_[1].iov_base = filePtr;
                        iov_[1].iov_len = remaining;
                        return -1;
                    } else if (wn == -2) {
                        // 致命错误
                        *saveErrno = errno;
                        return -1;
                    } else {
                        // 成功写入wn字节
                        filePtr += wn;
                        remaining -= (size_t)wn;
                    }
                }
                // 全部文件写完，清理状态
                writeBuff_.RetrieveAll();
                iov_[1].iov_base = nullptr;
                iov_[1].iov_len = 0;
            }

            // SSL模式成功完成所有写入
            len = 1;
        } else {
            // 普通HTTP模式：使用writev进行分散写入
            len = writev(fd_, iov_, iovCnt_);
            if(len <= 0) {
                *saveErrno = errno;
                break;
            }
            
            // 更新iovec结构，处理部分写入的情况
            if(static_cast<size_t>(len) > iov_[0].iov_len) {
                // 部分数据写入到第二个缓冲区（文件内容）
                iov_[1].iov_base = (uint8_t*) iov_[1].iov_base + (len - iov_[0].iov_len);
                iov_[1].iov_len -= (len - iov_[0].iov_len);
                if(iov_[0].iov_len) {
                    writeBuff_.RetrieveAll();  // 第一个缓冲区已完全发送
                    iov_[0].iov_len = 0;
                }
            }
            else {
                // 只写入了第一个缓冲区（HTTP头）
                iov_[0].iov_base = (uint8_t*)iov_[0].iov_base + len; 
                iov_[0].iov_len -= len; 
                writeBuff_.Retrieve(len);  // 从写缓冲区移除已发送的数据
            }
        }
    } while (isET && ToWriteBytes() > 0);  // ET模式且还有数据要写时继续循环
    return len;
}

/**
 * 处理HTTP请求并生成响应
 * 
 * 这是HTTP连接的核心处理方法，负责：
 * 1. 解析HTTP请求
 * 2. 处理聊天接口请求
 * 3. 调用LLM API获取回复
 * 4. 生成HTTP响应
 * 
 * @return true 响应准备就绪，false 需要更多数据
 */
bool HttpConn::process() {
    request_.Init();  // 初始化请求对象
    
    // 检查是否有足够的数据进行解析
    if(readBuff_.ReadableBytes() <= 0) { 
        return false;
    }
    else if(request_.parse(readBuff_)) {  // 解析HTTP请求
        LOG_DEBUG(MODULE_HTTP, "%s", request_.path().c_str());
        LOG_INFO(MODULE_HTTP, "Processing path: %s", request_.path().c_str());
        LOG_INFO(MODULE_HTTP, "Request method: %s", request_.method().c_str());
        LOG_INFO(MODULE_HTTP, "Request body: %s", request_.body().c_str());
        
        // 处理聊天接口请求
        if(request_.path() == "/chat") {
            LOG_INFO(MODULE_HTTP, "Chat endpoint accessed");
            
            // 尝试从POST参数中获取消息
            std::string message = request_.GetPost("message");
            LOG_INFO(MODULE_HTTP, "GetPost message: '%s'", message.c_str());
            
            // 如果POST参数中没有消息，尝试从JSON请求体中解析
            if(message.empty()) {
                std::string body = request_.body();
                LOG_INFO(MODULE_HTTP, "Request body: '%s'", body.c_str());
                
                // 使用简单的字符串查找方式解析JSON
                // 查找 "message": "..." 模式
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
                                // 提取引号之间的内容
                                message = body.substr(quote1 + 1, quote2 - quote1 - 1);
                                LOG_INFO(MODULE_HTTP, "Extracted message: '%s'", message.c_str());
                            }
                        }
                    }
                } else {
                    LOG_INFO(MODULE_HTTP, "Pattern 'message' not found in body");
                }
            }
            
            // 检查是否成功获取到消息
            if(message.empty()) {
                LOG_INFO(MODULE_HTTP, "No message provided, returning error");
                response_.SetContent("{\"error\": \"No message provided\"}", "application/json");
                response_.Init(srcDir, request_.path(), request_.IsKeepAlive(), 400);
            } else {
                LOG_INFO(MODULE_HTTP, "Processing message: '%s'", message.c_str());
                
                // 调用LLM API获取AI回复
                LOG_INFO(MODULE_HTTP, "Calling LLM API via HTTP");
                
                // 构建curl命令调用LLM API
                // 使用较长的超时时间以支持LLM推理
                std::string curl_cmd = "curl -s -X POST http://localhost:8000/chat "
                                     "-H \"Content-Type: application/json\" "
                                     "-d '{\"message\": \"" + message + "\"}' "
                                     "--max-time 30 --connect-timeout 5";
                
                LOG_INFO(MODULE_HTTP, "Executing curl command: %s", curl_cmd.c_str());
                std::string response = exec(curl_cmd.c_str());
                LOG_INFO(MODULE_HTTP, "LLM response: '%s'", response.c_str());
                
                std::string final_response;
                
                // 检查LLM API响应是否有效
                if(response.empty() || response.find("ERROR") != std::string::npos || response.find("error") != std::string::npos) {
                    LOG_INFO(MODULE_HTTP, "LLM service error, using fallback response");
                    // LLM API失败时，提供智能回退响应
                    final_response = "{\"response\": \"抱歉，AI服务暂时不可用。不过我可以告诉你，你刚才问的是：'" + message + "'。请稍后再试或联系管理员。\"}";
                } else {
                    LOG_INFO(MODULE_HTTP, "LLM service success, processing response");
                    // 确保响应是有效的JSON格式
                    if(response.find("{") == std::string::npos) {
                        // 如果响应不是JSON格式，包装成JSON
                        final_response = "{\"response\": \"" + response + "\"}";
                    } else {
                        final_response = response;
                    }
                }
                
                LOG_INFO(MODULE_HTTP, "Final response: %s", final_response.c_str());
                
                // 构建完整的HTTP响应
                std::string http_response = "HTTP/1.1 200 OK\r\n"
                                          "Content-Type: application/json\r\n"
                                          "Content-Length: " + std::to_string(final_response.length()) + "\r\n"
                                          "Connection: " + (request_.IsKeepAlive() ? "keep-alive" : "close") + "\r\n\r\n"
                                          + final_response;
                
                // 将响应写入缓冲区
                writeBuff_.Append(http_response);
                iov_[0].iov_base = const_cast<char*>(writeBuff_.Peek());
                iov_[0].iov_len = writeBuff_.ReadableBytes();
                iovCnt_ = 1;
                
                LOG_INFO(MODULE_HTTP, "Response set successfully, length: %zu", writeBuff_.ReadableBytes());
                LOG_INFO(MODULE_HTTP, "Final response: %s", final_response.c_str());
                return true; // 直接返回，不执行后续的MakeResponse
            }
        } else {
            // 处理其他HTTP请求（静态文件等）
            response_.Init(srcDir, request_.path(), request_.IsKeepAlive(), 200);
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