/***********************************************
 * FileName    : log.cpp
 * Description : see log.h
 * 
 * Feature     :
 * 
 * Author      : JasonGe
 * Created on  : 2025/03/30
************************************************/
#include "log.h"

using namespace std;

Log::Log() {
    lineCount_ = 0;
    isAsync_ = false;
    writeThread_ = nullptr;
    deque_ = nullptr;
    toDay_ = 0;
    fp_ = nullptr;
}

Log::~Log() {
    if(writeThread_ && writeThread_->joinable()) { // 如果是有写线程，即异步模式
        while(!deque_->empty()) {  // 字符串队列非空，立即刷新写入
            deque_->flush();
        };
        deque_->Close();   // 关闭队列
        writeThread_->join();  // 关闭写线程
    }
    if(fp_) {
        lock_guard<mutex> locker(mtx_);
        flush();     // 内存缓冲区数据立即写入文件
        fclose(fp_);
    }
}

int Log::GetLevel() {
    lock_guard<mutex> locker(mtx_);
    return level_;
}

/* 设置日志级别 */
void Log::SetLevel(int level) {
    lock_guard<mutex> locker(mtx_);
    level_ = level;
}

/* 初始化，创建日志文件 */
void Log::init(int level = 1, const char* path, const char* suffix,
    int maxQueueSize) {  //异步需要设置阻塞队列的长度，同步不需要设置
    isOpen_ = true;
    level_ = level;
    if(maxQueueSize > 0) {  // 如果设置了最大队列长度，则设置为异步
        isAsync_ = true;
        if(!deque_) {  // 如果队列为空
            unique_ptr<BlockDeque<std::string>> newDeque(new BlockDeque<std::string>);
            deque_ = move(newDeque);  // 初始化队列，用了智能指针的移动赋值运算符
            
            // 申请一个写线程，该写线程运行刷新日志写入函数，新建了线程来负责把消息写入文件，所以是异步写入
            std::unique_ptr<std::thread> NewThread(new thread(FlushLogThread));
            writeThread_ = move(NewThread);  // 初始化一个写线程
        }
    } else {
        isAsync_ = false;
    }

    lineCount_ = 0;

    time_t timer = time(nullptr);  // 获取时间，距1970年的秒数
    struct tm *sysTime = localtime(&timer);  // 转换为当地时区表示，年月日
    struct tm t = *sysTime;   // 复值转换后的值
    path_ = path;
    suffix_ = suffix;
    char fileName[LOG_NAME_LEN] = {0};
    snprintf(fileName, LOG_NAME_LEN - 1, "%s/%04d_%02d_%02d%s", 
            path_, t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, suffix_); // 日志文件的名称，按天分类
    toDay_ = t.tm_mday;   // 今天是每月的第几天

    {
        lock_guard<mutex> locker(mtx_);
        buff_.RetrieveAll();
        if(fp_) {   // 如果非空指针？
            flush();
            fclose(fp_); 
        }

        fp_ = fopen(fileName, "a");   // 创建/打开日志文件
        if(fp_ == nullptr) {    // 如果打开文件失败，可能是日志文件目录没有创建
            mkdir(path_, 0777);   // 创建日志文件目录
            fp_ = fopen(fileName, "a");  // 重新打开文件
        } 
        assert(fp_ != nullptr);
    }
}

/* 写日志，先判断要不要创建新日志文件，再往写缓冲区中写入信息，之后根据是否异步写日志文件 */
void Log::write(int level, const char *format, ...) {
    struct timeval now = {0, 0};   // (s, ms)
    gettimeofday(&now, nullptr);   //获取当前的系统时间
    time_t tSec = now.tv_sec;
    struct tm *sysTime = localtime(&tSec);  // 转换成当地时间的年月日
    struct tm t = *sysTime;
    va_list vaList;   // 初始化一个变参对象

    /* 日志日期 日志行数 */
    if (toDay_ != t.tm_mday || (lineCount_ && (lineCount_  %  MAX_LINES == 0)))
    {  // 如果日志不是今天（应该指日志的对象里的天数不是今天？）或者写入的日志行数是最大行的倍数，都要创建新文件
        unique_lock<mutex> locker(mtx_);
        locker.unlock();
        
        char newFile[LOG_NAME_LEN];
        char tail[36] = {0};   // 存储今天的年月日
        snprintf(tail, 36, "%04d_%02d_%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);

        if (toDay_ != t.tm_mday)   // 如果不是今天
        {
            snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s%s", path_, tail, suffix_); // 根据今天创建新文件名
            toDay_ = t.tm_mday;   // 改变成员变量为今天
            lineCount_ = 0;       // 重置行数为0
        }
        else {  // 否则说明文件写满了，新文件名要添加是当天的第几个日志文件
            snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s-%d%s", path_, tail, (lineCount_  / MAX_LINES), suffix_);
        }
        
        locker.lock();   // 后面的操作需要互斥
        flush();
        fclose(fp_);   // 关闭旧文件
        fp_ = fopen(newFile, "a");  // 打开新文件
        assert(fp_ != nullptr);
    }

    {
        unique_lock<mutex> locker(mtx_);
        lineCount_++;
        int n = snprintf(buff_.BeginWrite(), 128, "%d-%02d-%02d %02d:%02d:%02d.%06ld ",
                    t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                    t.tm_hour, t.tm_min, t.tm_sec, now.tv_usec);  // 往写缓冲区中先写入时间
                    
        buff_.HasWritten(n);   // 更新写缓冲区指针
        AppendLogLevelTitle_(level);  // 插入日志级别

        va_start(vaList, format);   // 获取format后面的所有可变参数，存储到vaList
        int m = vsnprintf(buff_.BeginWrite(), buff_.WritableBytes(), format, vaList); // 消息写入到缓冲区中
        va_end(vaList);

        buff_.HasWritten(m);  // 更新缓冲区写指针
        buff_.Append("\n\0", 2);    // 插入换行符和字符串结束的空字符

        if(isAsync_ && deque_ && !deque_->full()) {  // 如果要异步写入，且队列不满
            deque_->push_back(buff_.RetrieveAllToStr());   // 将缓冲区所有字符作为一个string插入到队列
        } else {
            fputs(buff_.Peek(), fp_);  // 否则是同步直接将字符串写入文件
        }
        buff_.RetrieveAll();   // 清空缓冲区
    }
}

/* 根据日志级别，确定要写入日志的级别标志 */
void Log::AppendLogLevelTitle_(int level) {
    switch(level) {
    case 0:
        buff_.Append("[debug]: ", 9);
        break;
    case 1:
        buff_.Append("[info] : ", 9);
        break;
    case 2:
        buff_.Append("[warn] : ", 9);
        break;
    case 3:
        buff_.Append("[error]: ", 9);
        break;
    default:
        buff_.Append("[info] : ", 9);
        break;
    }
}

/* 刷新文件流，让在内存缓冲区的数据立即写入文件 */
void Log::flush() {
    if(isAsync_) { // 如果是同步，调用队列的刷新函数把缓冲区数据读走
        deque_->flush(); 
    }
    fflush(fp_);  // 刷新文件流，确保数据写入文件
}

/* 将字符串阻塞队列的所有元素（即字符串）输出到文件 */
void Log::AsyncWrite_() {   
    string str = "";
    while(deque_->pop(str)) {   // 队列所有元素出队，每个元素代表write中的一条消息
        lock_guard<mutex> locker(mtx_);   // 对文件的操作互斥
        fputs(str.c_str(), fp_);   // 消息写入到文件中
    }
}

/* 单例懒汉模式 */
Log* Log::Instance() {
    static Log inst;
    return &inst;
}

/* 让阻塞队列的数据立即写入到文件 */
void Log::FlushLogThread() {
    Log::Instance()->AsyncWrite_();
}