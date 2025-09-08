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

int main() {
    /* 创建Web服务器实例，支持SSL/TLS加密通信
     * 参数说明：
     * - 端口: 1316 (HTTPS端口)
     * - 触发模式: 3 (ET模式)
     * - 超时时间: 60000ms
     * - 优雅退出: false
     * - MySQL配置: 端口3306, 用户root, 密码12345, 数据库jydb
     * - 连接池数量: 12, 线程池数量: 6
     * - 日志配置: 开启, 等级1, 队列容量1024
     * - SSL配置: 启用SSL, 证书文件server_chain.pem, 私钥文件server.key
     */
    WebServer server(
        1316, 3, 60000, false,             /* 端口 ET模式 timeoutMs 优雅退出  */
        3306, "root", "12345", "jydb",     /* MySQL数据库配置 */
        12, 6, true, 1, 1024,              /* 连接池数量 线程池数量 日志开关 日志等级 日志异步队列容量 */
        1, "./resources/server_chain.pem", "./resources/server.key");  /* SSL开关 证书文件 私钥文件 */
    server.Start();
} 
  
