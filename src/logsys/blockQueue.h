/***********************************************************
 * FileName    : blockQueue.h
 * Description : This header file define a class named BlockQueue, 
 *               which realize deque supporting block operation base
 *               on STL-deque.
 * 
 * Features    : 
 *    - add mutex to insure exclusive access for deque
 *    - add two condition_variable to realize block of pop and 
 *      push operation
 *    - set a max_capacity
 * 
 * Author      : JasonGe
 * Created on  : 2025/03/30
 * 
***********************************************************/
#ifndef BLOCKQUEUE_H
#define BLOCKQUEUE_H

// 在双端队列容器的基础上添加了条件变量，实现阻塞队列，STL::deque是非阻塞的，即如果取空队列的元素
// 会立即返回错误，而且其也没有容量限制，可以一直插入；阻塞队列则利用条件变量实现了往空队列中取元素时，会阻塞使用了pop的线程
// 直到被消费者变量唤醒；定义了最大容量，往一个满队列中插入时也会阻塞，直到被生产者变量唤醒；阻塞队列还加入了互斥锁
// 确保线程安全
#include <mutex>
#include <deque>
#include <condition_variable>
#include <sys/time.h>

template<class T>
class BlockDeque {
public:
    explicit BlockDeque(size_t MaxCapacity = 1000);

    ~BlockDeque();

    void clear();

    bool empty();

    bool full();

    void Close();

    size_t size();

    size_t capacity();

    T front();

    T back();

    void push_back(const T &item);

    void push_front(const T &item);

    bool pop(T &item);

    bool pop(T &item, int timeout);

    void flush();

private:
    std::deque<T> deq_;   // 双端队列

    size_t capacity_;

    std::mutex mtx_;

    bool isClose_;

    // 这里设置了两个条件变量，因为可能有多个生产者、消费者线程，如果只设置一个条件变量con，例如有两个生产者A, B，A先检
    // 查con，发现满足，于是消费一个资源，然后con.notify准备通知生产者可以生产，但是B线程也在等待con，A的notify也会通
    // 知B,导致B以为条件就绪，访问资源，但此时队列可能是空的，B被错误的条件变量唤醒了，所以需要两个条件变量
    std::condition_variable condConsumer_;

    std::condition_variable condProducer_;
};


template<class T>
BlockDeque<T>::BlockDeque(size_t MaxCapacity) :capacity_(MaxCapacity) {
    assert(MaxCapacity > 0);
    isClose_ = false;
}

template<class T>
BlockDeque<T>::~BlockDeque() {
    Close();
};

template<class T>
void BlockDeque<T>::Close() {
    {   
        std::lock_guard<std::mutex> locker(mtx_);  // 对队列的访问必须互斥
        deq_.clear();
        isClose_ = true;
    }
    condProducer_.notify_all();
    condConsumer_.notify_all();
};

/* 通知消费者线程取走缓冲区数据，用于把队列中的数据尽快写入到日志中 */
template<class T>
void BlockDeque<T>::flush() { 
    condConsumer_.notify_one();
};

template<class T>
void BlockDeque<T>::clear() {
    std::lock_guard<std::mutex> locker(mtx_);
    deq_.clear();
}

template<class T>
T BlockDeque<T>::front() {
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.front();
}

template<class T>
T BlockDeque<T>::back() {
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.back();
}

template<class T>
size_t BlockDeque<T>::size() {
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.size();
}

template<class T>
size_t BlockDeque<T>::capacity() {
    std::lock_guard<std::mutex> locker(mtx_);
    return capacity_;
}

/* 往队列尾中添加一个元素，模拟生产者 */
template<class T>
void BlockDeque<T>::push_back(const T &item) { //一个互斥锁+信号量+容量限制+deque的push_back
    std::unique_lock<std::mutex> locker(mtx_);
    while(deq_.size() >= capacity_) {  // 当队列长度超过容量
        condProducer_.wait(locker);   // 当前线程等待生产者条件变量
    }
    deq_.push_back(item);
    condConsumer_.notify_one();  // 队列中插入了任务，通知一个消费者线程
}

/* 往队列头添加一个元素，模拟生产者 */
template<class T>
void BlockDeque<T>::push_front(const T &item) {
    std::unique_lock<std::mutex> locker(mtx_);
    while(deq_.size() >= capacity_) {
        condProducer_.wait(locker);
    }
    deq_.push_front(item);
    condConsumer_.notify_one();
}

template<class T>
bool BlockDeque<T>::empty() {
    std::lock_guard<std::mutex> locker(mtx_);  // 队列的访问都要互斥
    return deq_.empty();
}

template<class T>
bool BlockDeque<T>::full(){
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.size() >= capacity_;
}

/* 从队头使一个元素出队，返回是否成功，阻塞等待，模拟消费者 */
template<class T>
bool BlockDeque<T>::pop(T &item) {
    std::unique_lock<std::mutex> locker(mtx_);
    while(deq_.empty()){  // 如果队列是空的
        condConsumer_.wait(locker);  // 本线程等待消费者条件变量
        if(isClose_){
            return false;
        }
    }
    item = deq_.front();
    deq_.pop_front();
    condProducer_.notify_one();  // 出队一个之后，有空间了，通知生产者线程
    return true;
}

/* 从队头使一个元素出队，最多等待timeout时间，阻塞超时等待 */
template<class T>
bool BlockDeque<T>::pop(T &item, int timeout) {
    std::unique_lock<std::mutex> locker(mtx_);
    while(deq_.empty()){
        if(condConsumer_.wait_for(locker, std::chrono::seconds(timeout)) 
                == std::cv_status::timeout){  // 如果等待超时，直接返回，实现阻塞超时
            return false;
        }
        if(isClose_){
            return false;
        }
    }
    item = deq_.front();
    deq_.pop_front();
    condProducer_.notify_one(); // 出队后有空间，通知生产者线程
    return true;
}

#endif // BLOCKQUEUE_H