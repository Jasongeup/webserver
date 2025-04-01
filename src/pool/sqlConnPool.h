/***********************************************************
 * FileName    : sqlConnPool.h
 * Description : This header file define a class named sqlConnPool
 *               which keep several connection with mysql-database.
 * 
 * Features    : 
 *   - connection queue having some connected database
 *   - get a sql-connection from connQueue
 *   - push a avaliable sql-connection into connQueue
 *   - use semaphore to control the apply of connection
 *   - use mutex to control the visit of connQueue
 * 
 * Author      : JasonGe
 * Created on  : 2025/03/29
 * 
***********************************************************/
#ifndef SQLCONNPOOL_H
#define SQLCONNPOOL_H

#include <mysql/mysql.h>
#include <string>
#include <queue>
#include <mutex>
#include <semaphore.h>
#include <thread>
#include "../logsys/log.h"

class SqlConnPool {
public:
    static SqlConnPool *Instance();

    MYSQL *GetConn();
    void FreeConn(MYSQL * conn);
    int GetFreeConnCount();

    void Init(const char* host, int port,
              const char* user,const char* pwd, 
              const char* dbName, int connSize);
    void ClosePool();

private:
    SqlConnPool();
    ~SqlConnPool();

    int MAX_CONN_;
    int useCount_;
    int freeCount_;

    std::queue<MYSQL *> connQue_;
    std::mutex mtx_;
    sem_t semId_;   // 信号量表示连接队列中数据库连接的数量
};


#endif // SQLCONNPOOL_H