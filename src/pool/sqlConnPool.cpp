/***********************************************************
 * FileName    : sqlConnPool.cpp
 * Description : see sqlConnPool.h.
 * 
 * Features    : 
 * 
 * Author      : JasonGe
 * Created on  : 2025/03/29
 * 
***********************************************************/

 #include "sqlConnPool.h"
 using namespace std;
 
 SqlConnPool::SqlConnPool() {
     useCount_ = 0;
     freeCount_ = 0;
 }
 
 /* 单例懒汉模式 */
 SqlConnPool* SqlConnPool::Instance() {  
     static SqlConnPool connPool;
     return &connPool;
 }
 
 /* 建立connSize个数据库连接并插入队列 */
 void SqlConnPool::Init(const char* host, int port,
             const char* user,const char* pwd, const char* dbName,
             int connSize = 10) {
     assert(connSize > 0);
     for (int i = 0; i < connSize; i++) {  // 往连接队列中初始化多个数据库连接
         MYSQL *sql = nullptr;
         sql = mysql_init(sql);
         if (!sql) {
             LOG_ERROR("MySql init error!");
             assert(sql);
         }
         sql = mysql_real_connect(sql, host,
                                  user, pwd,
                                  dbName, port, nullptr, 0); // 连接数据库
         if (!sql) {
             LOG_ERROR("MySql Connect error!");
         }
         connQue_.push(sql);  // 将连接的数据库插入到连接队列中
     }
     MAX_CONN_ = connSize;   // 初始化支持的最大连接数量
     sem_init(&semId_, 0, MAX_CONN_);   // 初始化信号量
 }
 
 /* 从连接队列中取出一个数据库连接 */
 MYSQL* SqlConnPool::GetConn() {
     MYSQL *sql = nullptr; 
     if(connQue_.empty()){   // 如果队列为空，没有连接的数据库
         LOG_WARN("SqlConnPool busy!");
         return nullptr;
     }
     sem_wait(&semId_);  // 等待空闲的数据库连接
     {
         lock_guard<mutex> locker(mtx_);  // 访问数据库连接队列需要加锁
         sql = connQue_.front();
         connQue_.pop();
     }
     return sql;
 }
 
 /* 数据库访问结束，回收数据库连接插入到队列中，增加信号量 */
 void SqlConnPool::FreeConn(MYSQL* sql) {  
     assert(sql);
     lock_guard<mutex> locker(mtx_);
     connQue_.push(sql);
     sem_post(&semId_);
 }
 
 /* 关闭数据库连接池 */
 void SqlConnPool::ClosePool() {
     lock_guard<mutex> locker(mtx_);
     while(!connQue_.empty()) {
         auto item = connQue_.front();
         connQue_.pop();
         mysql_close(item);
     }
     mysql_library_end();        
 }
 
 /* 获取数据库连接池空闲连接数量 */
 int SqlConnPool::GetFreeConnCount() {
     lock_guard<mutex> locker(mtx_);
     return connQue_.size();
 }
 
 SqlConnPool::~SqlConnPool() {
     ClosePool();
 }
 