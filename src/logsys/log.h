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
#ifndef LOG_H
#define LOG_H

#include <mutex>
#include <string>
#include <thread>
#include <sys/time.h>
#include <string.h>
#include <stdarg.h>           // vastart va_end
#include <assert.h>
#include <sys/stat.h>         //mkdir
#include "blockQueue.h"
#include "../buffer/buffer.h"

class Log {
public:
    void init(int level, const char* path = "./log", 
                const char* suffix =".log",
                int maxQueueCapacity = 1024);

    static Log* Instance();   //单例懒汉模式
    static void FlushLogThread();

    void write(int level, const char *format,...);
    void flush();

    int GetLevel();
    void SetLevel(int level);
    bool IsOpen() { return isOpen_; }
    
private:
    Log();
    void AppendLogLevelTitle_(int level);
    virtual ~Log();
    void AsyncWrite_();

private:
    static const int LOG_PATH_LEN = 256;
    static const int LOG_NAME_LEN = 256;
    static const int MAX_LINES = 50000;

    const char* path_;   // 日志文件目录路径
    const char* suffix_;  // 日志后缀

    int MAX_LINES_;

    int lineCount_;
    int toDay_;  // 今天是每月的第几天

    bool isOpen_;
 
    Buffer buff_;  // 日志的写缓冲区
    int level_;  // 日志级别，0-debug, 1-info, 2-warn, 3-error
    bool isAsync_;

    FILE* fp_;      // 日志文件的相对路径，每天都有一个日志文件
    std::unique_ptr<BlockDeque<std::string>> deque_; 
    std::unique_ptr<std::thread> writeThread_;   // 写线程
    std::mutex mtx_;
};

#define LOG_BASE(level, format, ...) \
    do {\
        Log* log = Log::Instance();\
        if (log->IsOpen() && log->GetLevel() <= level) {\
            log->write(level, format, ##__VA_ARGS__); \
            log->flush();\
        }\
    } while(0);

#define LOG_DEBUG(format, ...) do {LOG_BASE(0, format, ##__VA_ARGS__)} while(0);
#define LOG_INFO(format, ...) do {LOG_BASE(1, format, ##__VA_ARGS__)} while(0);
#define LOG_WARN(format, ...) do {LOG_BASE(2, format, ##__VA_ARGS__)} while(0);
#define LOG_ERROR(format, ...) do {LOG_BASE(3, format, ##__VA_ARGS__)} while(0);

#endif //LOG_H