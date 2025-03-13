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

#define MAX_EVENT_NUMBER  1024
#define BUFFER_SIZE       1024

/* 自定义结构体存储epoll与socket关联信息 */
struct fds {
    int epollfd;
    int sockfd;
};

/* 设置文件描述符为非阻塞模式 */
int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

/**
 * @brief 注册文件描述符到epoll实例
 * @param epollfd   epoll实例描述符
 * @param fd        目标文件描述符
 * @param oneshot   是否启用EPOLLONESHOT
 */
void addfd(int epollfd, int fd, bool oneshot)
{
    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;

    if (oneshot) {
        event.events |= EPOLLONESHOT;
    }

    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

/* 重置EPOLLONESHOT事件 */
void reset_oneshot(int epollfd, int fd)
{
    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

/* 工作线程函数 */
void* worker(void* arg)
{
    int sockfd = ((struct fds*)arg)->sockfd;
    int epollfd = ((struct fds*)arg)->epollfd;
    printf("Start processing fd:%d\n", sockfd);

    char buf[BUFFER_SIZE];
    while (1) {
        memset(buf, '\0', BUFFER_SIZE);
        int ret = recv(sockfd, buf, BUFFER_SIZE-1, 0);
        
        if (ret == 0) {
            close(sockfd);
            printf("Connection closed on fd:%d\n", sockfd);
            break;
        } else if (ret < 0) {
            if (errno == EAGAIN) {
                reset_oneshot(epollfd, sockfd);
                printf("Retry later on fd:%d\n", sockfd);
                break;
            }
        } else {
            printf("Received: %s\n", buf);
            sleep(5); // 模拟数据处理
        }
    }
    printf("End processing fd:%d\n", sockfd);
    return NULL;
}

int main(int argc, char* argv[])
{
    if (argc <= 2) {
        printf("Usage: %s <IP> <Port>\n", basename(argv[0]));
        return 1;
    }

    const char* ip = argv[1];
    int port = atoi(argv[2]);

    // 创建监听socket
    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);

    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

    // 绑定并监听
    int ret = bind(listenfd, (struct sockaddr*)&address, sizeof(address));
    assert(ret != -1);
    ret = listen(listenfd, 5);
    assert(ret != -1);

    // 创建epoll实例
    int epollfd = epoll_create(5);
    assert(epollfd != -1);
    addfd(epollfd, listenfd, false);

    struct epoll_event events[MAX_EVENT_NUMBER];
    while (1) {
        int event_count = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if (event_count < 0) {
            perror("epoll_wait error");
            break;
        }

        for (int i = 0; i < event_count; i++) {
            int sockfd = events[i].data.fd;
            
            if (sockfd == listenfd) { // 新连接
                struct sockaddr_in client_addr;
                socklen_t addr_len = sizeof(client_addr);
                int connfd = accept(listenfd, (struct sockaddr*)&client_addr, &addr_len);
                addfd(epollfd, connfd, true); // 对新连接启用EPOLLONESHOT
            } else if (events[i].events & EPOLLIN) { // 可读事件
                pthread_t tid;
                struct fds task;
                task.epollfd = epollfd;
                task.sockfd = sockfd;
                pthread_create(&tid, NULL, worker, (void*)&task);
            }
        }
    }
    
    close(listenfd);
    return 0;
}