/***********************************************
 * FileName    : heapTimer.cpp
 * Description : see heapTimer.h
 * 
 * Feature     :
 * 
 * Author      : JasonGe
 * Created on  : 2025/03/30
************************************************/
#include "heapTimer.h"

/* 最小堆的上滤操作，主要用于堆的插入 */
void HeapTimer::siftup_(size_t i) {
    assert(i >= 0 && i < heap_.size());
    size_t j = (i - 1) / 2;  // 父结点索引
    while(j >= 0) {
        if(heap_[j] < heap_[i]) { break; }  // 父节点小于本结点，符合小根堆，直接退出
        SwapNode_(i, j);   // 反之交换本结点与父结点
        i = j;          // 父节点成为新的本结点
        j = (i - 1) / 2;   // 重新计算本结点的父结点
    }
}

void HeapTimer::SwapNode_(size_t i, size_t j) {
    assert(i >= 0 && i < heap_.size());
    assert(j >= 0 && j < heap_.size());
    std::swap(heap_[i], heap_[j]);
    ref_[heap_[i].id] = i;
    ref_[heap_[j].id] = j;
} 

/* 最小堆的下滤操作，确保以index为根,且结点序号小于n的子树拥有最小堆性质 */
bool HeapTimer::siftdown_(size_t index, size_t n) {
    assert(index >= 0 && index < heap_.size());
    assert(n >= 0 && n <= heap_.size());
    size_t i = index;
    size_t j = i * 2 + 1;
    while(j < n) {
        if(j + 1 < n && heap_[j + 1] < heap_[j]) j++;
        if(heap_[i] < heap_[j]) break;
        SwapNode_(i, j);
        i = j;
        j = i * 2 + 1;
    }
    return i > index;
}

/* 往堆中插入序号为id的定时器，如果不存在，则建立新结点并插入，若已存在，则更改定时时间，调整堆 */
void HeapTimer::add(int id, int timeout, const TimeoutCallBack& cb) {
    assert(id >= 0);
    size_t i;
    if(ref_.count(id) == 0) {
        /* 新节点：堆尾插入，调整堆 */
        i = heap_.size();
        ref_[id] = i;
        heap_.push_back({id, Clock::now() + MS(timeout), cb});
        siftup_(i);
    } 
    else {
        /* 已有结点：调整堆 */
        i = ref_[id];
        heap_[i].expires = Clock::now() + MS(timeout);
        heap_[i].cb = cb;
        // 如果下滤操作返回true，表明该定时器值下滤了，表明更改的值挺大就不用上滤了
        if(!siftdown_(i, heap_.size())) { 
            siftup_(i);   // 反之说明更改的值很小，则要上滤，看它是否比它的根节点还要小
        }
    }
}

/* 删除指定id结点，并触发回调函数 */
void HeapTimer::doWork(int id) {
    if(heap_.empty() || ref_.count(id) == 0) {
        return;
    }
    size_t i = ref_[id];
    TimerNode node = heap_[i];
    node.cb();
    del_(i);
}

/* 删除堆中指定位置的结点 */
void HeapTimer::del_(size_t index) {

    assert(!heap_.empty() && index >= 0 && index < heap_.size());
    /* 将要删除的结点换到队尾，然后调整堆 */
    size_t i = index;
    size_t n = heap_.size() - 1;
    assert(i <= n);
    if(i < n) {
        SwapNode_(i, n);
        if(!siftdown_(i, n)) {
            siftup_(i);
        }
    }
    /* 队尾元素删除，除了要从堆中弹出，还要从哈希表中删除 */
    ref_.erase(heap_.back().id);
    heap_.pop_back();
}

/* 调整指定id的结点的定时时间 */
void HeapTimer::adjust(int id, int timeout) {
    assert(!heap_.empty() && ref_.count(id) > 0);
    heap_[ref_[id]].expires = Clock::now() + MS(timeout);;
    siftdown_(ref_[id], heap_.size());
}

/* 心搏函数，检查堆顶元素是否定时时间超时 */
void HeapTimer::tick() {
    /* 清除超时结点 */
    if(heap_.empty()) {
        return;
    }
    while(!heap_.empty()) {
        TimerNode node = heap_.front();
        // 时间间隔大于0，表示还没超时，直接退出
        if(std::chrono::duration_cast<MS>(node.expires - Clock::now()).count() > 0) {
            break; 
        }
        node.cb();  // 否则就一直弹出堆顶元素，知道堆顶元素不超时
        pop();
    }
}

/* 弹出堆顶元素 */
void HeapTimer::pop() {
    assert(!heap_.empty());
    del_(0);
}

/* 清空堆、哈希表元素 */
void HeapTimer::clear() {
    ref_.clear();
    heap_.clear();
}

/* 运行一次心搏函数，并获取下次堆顶元素超时还需要的时间 */
int HeapTimer::GetNextTick() {
    tick();  
    size_t res = -1;
    if(!heap_.empty()) {
        res = std::chrono::duration_cast<MS>(heap_.front().expires - Clock::now()).count();
        if(res < 0) { res = 0; }
    }
    return res;
}