/***********************************************
 * FileName    : test.cpp
 * Description : do unit test
 * 
 * Feature     :
 * 
 * Author      : JasonGe
 * Created on  : 2025/03/31
************************************************/
#include "../src/logsys/log.h"
#include "../src/pool/threadPool.h"
#include <features.h>

#if __GLIBC__ == 2 && __GLIBC_MINOR__ < 30
#include <sys/syscall.h>
#define gettid() syscall(SYS_gettid)
#endif

/* 测试日志能否正确输出 */
void TestLog() {
    int cnt = 0, level = 0;
    Log::Instance()->init(level, "./testlog1", ".log", 0);  // 初始化一个测试同步日志实例
    for(level = 3; level >= 0; level--) {
        Log::Instance()->SetLevel(level);
        for(int j = 0; j < 10000; j++ ){
            for(int i = 0; i < 4; i++) {
                LOG_BASE(i,"%s 111111111 %d ============= ", "Test", cnt++);
            }
        }
    }
    cnt = 0;
    Log::Instance()->init(level, "./testlog2", ".log", 5000);  // 初始化一个测试异步日志实例
    for(level = 0; level < 4; level++) {
        Log::Instance()->SetLevel(level);
        for(int j = 0; j < 10000; j++ ){
            for(int i = 0; i < 4; i++) {
                LOG_BASE(i,"%s 222222222 %d ============= ", "Test", cnt++);
            }
        }
    }
}

/* 在测试线程池中允许的函数 */
void ThreadLogTask(int i, int cnt) {
    for(int j = 0; j < 10000; j++ ){
        LOG_BASE(i,"PID:[%04d]======= %05d ========= ", gettid(), cnt++);
    }
}

/* 测试线程池 */
void TestThreadPool() {
    Log::Instance()->init(0, "./testThreadpool", ".log", 5000);  /* 利用日志输出来查看线程池是否允许成功 */
    ThreadPool threadpool(6);
    for(int i = 0; i < 18; i++) {
        threadpool.AddTask(std::bind(ThreadLogTask, i % 4, i * 10000));
    }
    getchar();
}

int main() {
    TestLog();
    TestThreadPool();
}