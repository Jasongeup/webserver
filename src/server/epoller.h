/***********************************************************
 * FileName    : epoller.h
 * Description : This header file define a class named Epoller, 
 *               which pack epoll-operation.
 * 
 * Features    : 
 *    - create epoll_fd
 *    - add, mod, del epoll_event
 *    - epoll_wait
 * 
 * Author      : JasonGe
 * Created on  : 2025/03/30
 * 
***********************************************************/
#ifndef EPOLLER_H
#define EPOLLER_H

#include <sys/epoll.h> //epoll_ctl()
#include <fcntl.h>  // fcntl()
#include <unistd.h> // close()
#include <assert.h>
#include <vector>
#include <errno.h>

class Epoller {
public:
    explicit Epoller(int maxEvent = 1024);

    ~Epoller();

    bool AddFd(int fd, uint32_t events);

    bool ModFd(int fd, uint32_t events);

    bool DelFd(int fd);

    int Wait(int timeoutMs = -1);

    int GetEventFd(size_t i) const;

    uint32_t GetEvents(size_t i) const;
        
private:
    int epollFd_;

    std::vector<struct epoll_event> events_;    
};

#endif //EPOLLER_H