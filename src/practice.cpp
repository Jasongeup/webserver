#include<sys/types.h>
#include<sys/socket.h>
#include<bits/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<signal.h>
#include<unistd.h>
#include<stdlib.h>
#include<assert.h>
#include<stdio.h>
#include<string.h>
#include<iostream>
/*
struct sockaddr{
    sa_family_t sa_family;
    char sa_data[14];
}
int socket(int domain, int type, int protocol);
int bind(int sockfd, const struct sockaddr* my_addr, socklen_t addrlen);
*/
static bool stop=false;
// sigterm信号的处理函数，触发时结束主程序中的循环
static void handle_term(int sig){
    stop = true;
}
int main(int argc, char* argv[]){   // 在终端执行命令时，argc代表参数个数，包括程序名本身，例如程序名myc,在终端执行./myc 1, 3, 则argc=3, argv[0] = "./myc", argv[1] = "1"
    using namespace std;
    signal(SIGTERM, handle_term);
    if(argc <= 3){   // 如果参数个数小于3,说明输入格式不对
        cout << "usage:" << basename(argv[0]) << " ip_address port_number backlog" << endl;
        return 1;
    }
    const char* ip = argv[1];
    int port = atoi(argv[2]);
    int backlog = atoi(argv[3]);
    int sock = socket(PF_INET, SOCK_STREAM, 0);
    assert(sock >= 0);
    // 创建一个ipv4 socket地址
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);
    int ret = bind(sock, (struct sockaddr*)&address, sizeof(address));
    assert(ret != -1);
    ret = listen(sock, backlog);
    assert(ret != -1);
    //循环等待连接，直到有SIGTERM信号将它中断
    while(!stop){
        sleep(1);
    }
    //关闭socket
    close(sock);
    return 0;
}
