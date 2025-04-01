/***********************************************************
 * FileName    : sqlConnRAII.h
 * Description : This header file define a class named SqlConnRAII
 *               which make an sql-connection conform to RAII.
 * 
 * Features    : 
 *   - apply for a sql-connection from sqlConnPool when initialize
 *   - free sql-connection when destory
 * 
 * Author      : JasonGe
 * Created on  : 2025/03/28
 * 
***********************************************************/

 #ifndef SQLCONNRAII_H
 #define SQLCONNRAII_H
 #include "sqlConnPool.h"
 
 /* 资源（数据库连接）在对象构造初始化 资源在对象析构时释放*/
 class SqlConnRAII {
 public:
     /* 从connpool连接池里获取sql连接 */
     SqlConnRAII(MYSQL** sql, SqlConnPool *connpool) {
         assert(connpool);
         *sql = connpool->GetConn();
         sql_ = *sql;
         connpool_ = connpool;
     }
     
     /* 释放sql连接 */
     ~SqlConnRAII() {
         if(sql_) { connpool_->FreeConn(sql_); }
     }
     
 private:
     MYSQL *sql_;
     SqlConnPool* connpool_;
 };
 
 #endif //SQLCONNRAII_H