/***********************************************************
 * FileName    : threadPool.h
 * Description : This header file define a class named threadPool.
 * 
 * Features    : 
 *   - create a thread pool including a task queue
 *   - half-sync/half-reactive mode
 *   - use shared_ptr, condition_variable
 * 
 * Author      : JasonGe
 * Created on  : 2025/03/28
 * 
***********************************************************/

 #ifndef THREADPOOL_H
 #define THREADPOOL_H
 
 #include <mutex>
 #include <condition_variable>
 #include <queue>
 #include <thread>
 #include <functional>
 class ThreadPool {
 public:
     explicit ThreadPool(size_t threadCount = 8): pool_(std::make_shared<Pool>()) {
             assert(threadCount > 0);
             for(size_t i = 0; i < threadCount; i++) {
                 std::thread([pool = pool_] {    // 创建多个线程，pool_是智能指针，这里捕获它之后会令引用计数+1
                     std::unique_lock<std::mutex> locker(pool->mtx);  // 对线程池的访问加锁
                     while(true) {
                         if(!pool->tasks.empty()) {   // 如果队列非空
                             auto task = std::move(pool->tasks.front()); // 取出队头队列
                             pool->tasks.pop();
                             locker.unlock();   // 这时候要解锁，以便其他线程访问线程池对象
                             task();
                             locker.lock();   // 这时候要加锁，因为马上又到了本线程访问先线程池
                         } 
                         else if(pool->isClosed) break;  // 如果要关闭线程池
                         else pool->cond.wait(locker);   // 如果队列为空，且不关闭线程池，则等待信号量
                     }
                 }).detach();  // 设置脱离线程
             }
     }
 
     ThreadPool() = default;
 
     ThreadPool(ThreadPool&&) = default;
     
     /* 线程池对象本身是不用释放的，因为使用了智能指针，析构函数主要是通知各线程关闭 */
     ~ThreadPool() {
         if(static_cast<bool>(pool_)) {  // 如果线程池指针非空
             {
                 std::lock_guard<std::mutex> locker(pool_->mtx);
                 pool_->isClosed = true;  // 该成员的设置需要互斥
             }
             pool_->cond.notify_all(); // 当线程收到信号量被唤醒后，监测到isClosed=true，就会关闭自己
         }
     }
 
     template<class F>
     void AddTask(F&& task) {
         {
             std::lock_guard<std::mutex> locker(pool_->mtx);  // 对线程池的访问加锁
             pool_->tasks.emplace(std::forward<F>(task));
         }
         pool_->cond.notify_one();
     }
 
 private:
     struct Pool {
         std::mutex mtx;   // 互斥锁
         std::condition_variable cond;  // 条件变量
         bool isClosed;   // 是否关闭线程池
         std::queue<std::function<void()>> tasks;  // 任务队列，队列中的元素是返回值为空，没有参数的函数对象
     };
     std::shared_ptr<Pool> pool_;   // 线程池的引用计数
 };
 
 
 #endif //THREADPOOL_H