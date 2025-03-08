#include<sys/types.h>  // size_t, ssize_t, socklen_t
#include<sys/socket.h> // recv, send, shutdown, connect, listen, bind, socket， sockatmark
#include<bits/socket.h> // sockaddr
#include<netinet/in.h> // INET_ADDRSTRLEN, htonl, htons
#include<arpa/inet.h>  // inet_pton, inet_ntop
#include<signal.h>
#include<unistd.h>   // close, pipe, dup
#include<stdlib.h>
#include<assert.h>
#include<stdio.h>
#include<string.h> // basename
#include<iostream>
#include<netdb.h>  // gethostbyname,getserverbyname, getaddrinfo
#define BUFFER_SIZE 1024
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
/*listen
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
}*/
/*accept
int main(int argc, char* argv[]){   // 在终端执行命令时，argc代表参数个数，包括程序名本身，例如程序名myc,在终端执行./myc 1, 3, 则argc=3, argv[0] = "./myc", argv[1] = "1"
    using namespace std;
    if(argc <= 2){   // 如果参数个数小于3,说明输入格式不对
        cout << "usage:" << basename(argv[0]) << " ip_address port_number" << endl;
        return 1;
    }
    const char* ip = argv[1];
    int port = atoi(argv[2]);
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
    ret = listen(sock, 5);
    assert(ret != -1);
    //循环等待连接，直到有SIGTERM信号将它中断
    sleep(20);
    struct sockaddr_in client;
    socklen_t client_addrlength = sizeof(client);
    int connfd = accept(sock, (struct sockaddr*)&client, &client_addrlength);
    if(connfd < 0){
        cout << "errno is:" << errno << "\n" << endl;
    }
    else{
        char remote[INET_ADDRSTRLEN];
        cout << "connected with ip:" << inet_ntop(AF_INET, &client.sin_addr, remote, INET_ADDRSTRLEN) << "and port:" << ntohs(client.sin_port) << endl;
        close(connfd);
    }
    //关闭socket
    close(sock);
    return 0;
}*/
/*发送带外数据，TCP写数据,connect发起连接
int main(int argc, char* argv[]){   // 在终端执行命令时，argc代表参数个数，包括程序名本身，例如程序名myc,在终端执行./myc 1, 3, 则argc=3, argv[0] = "./myc", argv[1] = "1"
    using namespace std;
    if(argc <= 2){   // 如果参数个数小于3,说明输入格式不对
        cout << "usage:" << basename(argv[0]) << " ip_address port_number" << endl;
        return 1;
    }
    const char* ip = argv[1];
    int port = atoi(argv[2]);
    int sockfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(sockfd >= 0);
    // 创建一个ipv4 socket地址
    struct sockaddr_in server_address;
    bzero(&server_address, sizeof(server_address));
    server_address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &server_address.sin_addr);
    server_address.sin_port = htons(port);
    if(connect(sockfd, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) cout << "connection failed" << endl;
    else{
        const char* oob_data = "abc";
        const char* normal_data = "123";
        send(sockfd, normal_data, strlen(normal_data), 0);
        send(sockfd, oob_data, strlen(oob_data), MSG_OOB);
        send(sockfd, normal_data, strlen(normal_data), 0);
    }
    //关闭socket
    close(sockfd);
    return 0;
}*/
/* tcp读数据，设置SO_REUSEADDR
int main(int argc, char* argv[]){   // 在终端执行命令时，argc代表参数个数，包括程序名本身，例如程序名myc,在终端执行./myc 1, 3, 则argc=3, argv[0] = "./myc", argv[1] = "1"
    using namespace std;
    if(argc <= 2){   // 如果参数个数小于3,说明输入格式不对
        cout << "usage:" << basename(argv[0]) << " ip_address port_number" << endl;
        return 1;
    }
    const char* ip = argv[1];
    int port = atoi(argv[2]);
    int sock = socket(PF_INET, SOCK_STREAM, 0);
    assert(sock >= 0);
    // 设置可重用地址
    assert(sock >= 0);
    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    // 创建一个ipv4 socket地址
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);
    int ret = bind(sock, (struct sockaddr*)&address, sizeof(address));
    assert(ret != -1);
    ret = listen(sock, 5);
    assert(ret != -1);
    struct sockaddr_in client;
    socklen_t client_addrlength = sizeof(client);
    int connfd = accept(sock, (struct sockaddr*)&client, &client_addrlength);
    if(connfd < 0){
        cout << "errno is:" << errno << "\n" << endl;
    }
    else{
        char buffer[BUF_SIZE];
        memset(buffer, '\0', BUF_SIZE);
        ret = recv(connfd, buffer, BUF_SIZE-1, 0);
        cout << "got" << ret << "bytes of normal data '" << buffer << "'" << endl;
        memset(buffer, '\0', BUF_SIZE);
        ret = recv(connfd, buffer, BUF_SIZE-1, MSG_OOB);
        cout << "got" << ret << "bytes of oob data '" << buffer << "'" << endl;
        memset(buffer, '\0', BUF_SIZE);
        ret = recv(connfd, buffer, BUF_SIZE-1, 0);
        cout << "got" << ret << "bytes of normal data '" << buffer << "'" << endl;
        close(connfd);
    }
    //关闭socket
    close(sock);
    return 0;
}*/
/*修改TCP发送缓冲区, 运行在客户端，所以无需listen
int main(int argc, char* argv[]){   // 在终端执行命令时，argc代表参数个数，包括程序名本身，例如程序名myc,在终端执行./myc 1, 3, 则argc=3, argv[0] = "./myc", argv[1] = "1"
    using namespace std;
    if(argc <= 3){   // 如果参数个数小于3,说明输入格式不对
        cout << "usage:" << basename(argv[0]) << " ip_address port_number send_buffer_size" << endl;
        return 1;
    }
    const char* ip = argv[1];
    int port = atoi(argv[2]);
    // 创建一个ipv4 socket地址
    struct sockaddr_in server_address;
    bzero(&server_address, sizeof(server_address));
    server_address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &server_address.sin_addr);
    server_address.sin_port = htons(port);
    int sock = socket(PF_INET, SOCK_STREAM, 0);
    assert(sock >= 0);
    int sendbuf = atoi(argv[3]);
    int len = sizeof(sendbuf);
    // 先设置TCP发送缓冲区的大小，然后立即读取
    setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &sendbuf, sizeof(sendbuf));
    getsockopt(sock, SOL_SOCKET, SO_SNDBUF, &sendbuf, (socklen_t*)&len);
    cout << "the tcp send buffer size after setting is" << sendbuf << endl;
    if(connect(sock, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) cout << "connection failed" << endl;
    else{
        char buffer[BUFFER_SIZE];
        memset(buffer, 'a', BUFFER_SIZE);
        send(sock, buffer, BUFFER_SIZE, 0);
    }
    //关闭socket
    close(sock);
    return 0;
}*/
/*设置TCP接收缓冲区大小
int main(int argc, char* argv[]){   // 在终端执行命令时，argc代表参数个数，包括程序名本身，例如程序名myc,在终端执行./myc 1, 3, 则argc=3, argv[0] = "./myc", argv[1] = "1"
    using namespace std;
    if(argc <= 3){   // 如果参数个数小于3,说明输入格式不对
        cout << "usage:" << basename(argv[0]) << " ip_address port_number send_buffer_size" << endl;
        return 1;
    }
    const char* ip = argv[1];
    int port = atoi(argv[2]);
    // 创建一个ipv4 socket地址
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);
    int sock = socket(PF_INET, SOCK_STREAM, 0);
    assert(sock >= 0);
    int recvbuf = atoi(argv[3]);
    int len = sizeof(recvbuf);
    // 先设置TCP接受缓冲区的大小，然后立即读取
    setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &recvbuf, sizeof(recvbuf));
    getsockopt(sock, SOL_SOCKET, SO_RCVBUF, &recvbuf, (socklen_t*)&len);
    cout << "the tcp receive buffer size after setting is" << recvbuf << endl;
    int ret = bind(sock, (struct sockaddr*)&address, sizeof(address));
    assert(ret != -1);
    ret = listen(sock, 5);
    assert(ret != -1);
    struct sockaddr_in client;
    socklen_t client_addrlength = sizeof(client);
    int connfd = accept(sock, (struct sockaddr*)&client, &client_addrlength);
    if(connfd < 0){
        cout << "errno is:" << errno << "\n" << endl;
    }
    else{
        char buffer[BUFFER_SIZE];
        memset(buffer, '\0', BUFFER_SIZE);
        while(recv(connfd, buffer, BUFFER_SIZE-1, 0) > 0){}
        close(connfd);
    }
    //关闭socket
    close(sock);
    return 0;
}*/
int main(int argc, char*argv[]){
    using namespace std;
    assert(argc == 2);
    char* host = argv[1];
    //获取目标主机信息
    struct hostent* hostinfo = gethostbyname(host);
    assert(hostinfo);
    //获取daytime服务信息
    struct servent* servinfo = getservbyname("daytime", "tcp");
    assert(servinfo);
    cout << "daytime port is" << ntohs(servinfo->s_port) << endl;
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_port = servinfo->s_port;
    address.sin_addr = *(struct in_addr*)*hostinfo->h_addr_list;
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    int result = connect(sockfd, (struct sockaddr*)&address, sizeof(address));
    assert(result != -1);
    char buffer[128];
    result = read(sockfd, buffer, sizeof(buffer));
    assert(result > 0);
    buffer[result] = '\0';
    cout << "the day time is" << buffer;
    close(sockfd);
    return 0;
}
