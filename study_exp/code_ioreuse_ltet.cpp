#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<assert.h>
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<errno.h>
#include<string.h>
#include<fcntl.h>
#include<iostream>
#include<sys/epoll.h>
#include<pthread.h>
using namespace std;

/*
// select 系统调用处理带外数据
int main(int argc, char* argv[]){
    if(argc <= 2){
        cout << "usage:" << basename(argv[0]) << "ip_address port_number\n";
        return 1;
    }
    const char* ip = argv[1];
    int port = atoi(argv[2]);
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);
    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd > 0);
    int ret = bind(listenfd, (struct sockaddr*)&address, sizeof(address));
    assert(ret != -1);
    ret = listen(listenfd, 5);
    assert(ret != -1);
    struct sockaddr_in client_address;
    socklen_t client_addrlength = sizeof(client_address);
    int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_addrlength);
    if(connfd < 0){
        cout << "errno is :" << errno << endl;
        close(listenfd);
    }
    char buf[1024];
    fd_set read_fds;
    fd_set exception_fds;
    FD_ZERO(&read_fds);
    FD_ZERO(&exception_fds);
    while(1){
        memset(buf, '\0', sizeof(buf));
        //每次调用select前都要重新在read_fds和exception_fds中设置文件描述符
        //connfd，因为事件发生之后，文件描述符集合将被内核修改
        FD_SET(connfd, &read_fds);
        FD_SET(connfd, &exception_fds);
        ret = select(connfd + 1, &read_fds, NULL, &exception_fds, NULL);
        if(ret < 0){
            cout << "selection failure" << endl;
            break;
        }
        // 对于可读事件，采用普通的recv函数读取数据
        if(FD_ISSET(connfd, &read_fds)){
            ret = recv(connfd, buf, sizeof(buf) - 1, 0);
            if(ret <= 0) break;
            cout << "get" << ret << " bytes of normal data:" << buf << endl;
        }
        // 对于异常事件，采用带MSG_OOB标志的recv函数读取带外数据
        else if(FD_ISSET(connfd, &exception_fds)){
            ret = recv(connfd, buf, sizeof(buf) - 1, MSG_OOB);
            if(ret <= 0) break;
            cout << "get" << ret << " bytes of oob data:" << buf << endl;
        }
    }
    close(connfd);
    close(listenfd);
    return 0;
}*/

// LT和ET模式
#define MAX_EVENT_NUMBER 1024
#define BUFFER_SIZE 10

//将文件描述符设置成非阻塞的
int setnonblocking(int fd){
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

//将文件描述符fd上的EPOLLIN注册到epollfd指示的epoll内核事件表中，参数
//enable_et指定是否对fd启用ET模式
void addfd(int epollfd, int fd, bool enable_et){
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN;
    if(enable_et) event.events |= EPOLLET;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

//LT模式的工作流程
void lt(epoll_event* events, int number, int epollfd, int listenfd){
    char buf[BUFFER_SIZE];
    for(int i = 0; i < number; i++){
        int sockfd = events[i].data.fd;
        if(sockfd = listenfd){   // 就绪文件描述符刚好等于本地端口要监听的socket
            struct sockaddr_in client_address;
            socklen_t client_addrlength = sizeof(client_address);
            int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_addrlength);
            addfd(epollfd, connfd, false);   // 对connfd禁用et模式
            cout << "test1" << endl;
        }
        else if(events[i].events & EPOLLIN){
            // 只要socket读缓存中还有未读出的数据，这段代码就被触发
            cout << "event trigger once" << endl;
            memset(buf, '\0', BUFFER_SIZE);
            int ret = recv(sockfd, buf, BUFFER_SIZE-1, 0);
            if(ret <= 0){   // 代表sockfd中没有数据可读了，则关闭该socket
                close(sockfd);
                continue;
            }
            cout << "get" << ret << " bytes of normal data:" << buf << endl;
        }
        else{
            cout << "something else happened" << endl;
        }
    }
}

// ET模式的工作流程
void et(epoll_event* events, int number, int epollfd, int listenfd){
    char buf[BUFFER_SIZE];
    for(int i = 0; i < number; i++){
        int sockfd = events[i].data.fd;
        if(sockfd == listenfd){
            struct sockaddr_in client_address;
            socklen_t client_addrlength = sizeof(client_address);
            int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_addrlength);
            addfd(epollfd, connfd, false);   // 对connfd禁用et模式
        }
        else if(events[i].events & EPOLLIN){
            // 这段代码不会被重复触发，所以我们循环读取数据，以确保把socket读缓存中的所有数据读出
            cout << "event trigger once" << endl;
            while(1){
                memset(buf, '\0', BUFFER_SIZE);
                int ret = recv(sockfd, buf, BUFFER_SIZE-1, 0);
                if(ret < 0){
                    //对于非阻塞IO，下面的条件成立表示数据已经全部读取完毕。此后，epoll就能再次
                    //触发sockfd上的EPOLLIN事件，以驱动下一次读操作
                    if((errno == EAGAIN) || (errno) == EWOULDBLOCK){
                        cout << "read later" << endl;
                        break;
                    }
                    close(sockfd);
                    break;
                }
                else if(ret == 0){   // recv返回0,代表通信对方已经关闭连接了
                    close(sockfd);
                }
                else{
                    cout << "get" << ret << " bytes of normal data:" << buf << endl;
                }
            }
        }  
        else{
            cout << "something else happened" << endl;
        }
    }
}

int main(int argc, char* argv[]){
    if(argc <= 2){
        cout << "usage:" << basename(argv[0]) << "ip_address port_number\n";
        return 1;
    }
    const char* ip = argv[1];
    int port = atoi(argv[2]);
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);
    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd > 0);
    int ret = bind(listenfd, (struct sockaddr*)&address, sizeof(address));
    assert(ret != -1);
    ret = listen(listenfd, 5);
    assert(ret != -1);
    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);
    assert(epollfd != -1);
    addfd(epollfd, listenfd, true);
    while(1){
        int ret = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if(ret < 0){
            cout << "epoll failure" << endl;
            break;
        }
        lt(events, ret, epollfd, listenfd);  // 使用LT模式
        //et(events, ret, epollfd, listenfd); //使用ET模式
    }
    close(listenfd);
    return 0;
}