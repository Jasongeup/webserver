/***********************************************
 * FileName    : main.cpp
 * Description : run webserver
 * 
 * Feature     :
 * 
 * Author      : JasonGe
 * Created on  : 2025/03/30
************************************************/
#include <unistd.h>
#include <iostream>
#include <libgen.h>
#include "server/webserver.h"
#include "chat_handler.h"

int main() {
    WebServer server(
        1316, 3, 60000, false,             /* 端口 ET模式 timeoutMs 优雅退出  */
        3306, "root", "12345", "jydb", /* Mysql配置 */
        12, 6, true, 1, 1024,         /* 连接池数量 线程池数量 日志开关 日志等级 日志异步队列容量 */
        1, "./resources/server_chain.pem", "./resources/server.key");             
    
    // 添加聊天路由
    server.addRoute("/chat", chat_handler, POST);
    server.Start();
} 
  
