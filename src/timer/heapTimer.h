/***********************************************************
 * FileName    : heapTimer.h
 * Description : This header file define a class named HeapTimer, 
 *               which represent a minist heap to save timer.
 * 
 * Features    : 
 *    - define a timer-node, which includs expire time, callback()
 *    - can add, delete, adjust timer-node, and adjust heap automatically
 *    - check if over time by tick()
 * 
 * Author      : JasonGe
 * Created on  : 2025/03/30
 * 
***********************************************************/
#ifndef HEAP_TIMER_H
#define HEAP_TIMER_H

// 没有直接使用priority_queue容器是因为其不支持更改指定元素，不支持删除指定元素
#include <queue>
#include <unordered_map>
#include <time.h>
#include <algorithm>
#include <arpa/inet.h> 
#include <functional> 
#include <assert.h> 
#include <chrono>
#include "../logsys/log.h"

typedef std::function<void()> TimeoutCallBack;
typedef std::chrono::high_resolution_clock Clock;
typedef std::chrono::milliseconds MS;
typedef Clock::time_point TimeStamp;

struct TimerNode {
    int id;
    TimeStamp expires;  // 超时时间
    TimeoutCallBack cb;   // 超时回调函数
    bool operator<(const TimerNode& t) { // 重载了<运算符
        return expires < t.expires;
    }
};
class HeapTimer {
public:
    HeapTimer() { heap_.reserve(64); }  // 这里只申请内存，不初始化结点

    ~HeapTimer() { clear(); }
    
    void adjust(int id, int newExpires);

    void add(int id, int timeOut, const TimeoutCallBack& cb);

    void doWork(int id);

    void clear();

    void tick();

    void pop();

    int GetNextTick();

private:
    void del_(size_t i);
    
    void siftup_(size_t i);

    bool siftdown_(size_t index, size_t n);

    void SwapNode_(size_t i, size_t j);

    std::vector<TimerNode> heap_;

    std::unordered_map<int, size_t> ref_;   // 定时器序号，在堆中索引
};

#endif //HEAP_TIMER_H