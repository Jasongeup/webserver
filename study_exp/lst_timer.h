#ifndef LST_TIMER
#define LST_TIMER
#include <netinet/in.h>
#include <stdio.h>
#include <time.h>

#define BUFFER_SIZE 64

class util_timer; /* 前向声明 */

/* 用户数据结构：客户端 socket 地址、socket 文件描述符、读缓存和定时器 */
struct client_data {
    sockaddr_in address;
    int sockfd;
    char buf[BUFFER_SIZE];
    util_timer* timer;
};

/* 定时器类 */
class util_timer {
public:
    util_timer() : prev(nullptr), next(nullptr) {}

public:
    time_t expire; /* 任务的超时时间，这里使用绝对时间 */
    void (*cb_func)(client_data*); /* 任务回调函数 */
    
    /* 回调函数处理的客户数据，由定时器的执行者传递给回调函数 */
    client_data* user_data;
    
    util_timer* prev; /* 指向前一个定时器 */
    util_timer* next; /* 指向下一个定时器 */
};

/* 定时器链表：它是一个升序、双向链表，且带有头结点和尾节点 */
class sort_timer_lst {
public:
    sort_timer_lst() : head(nullptr), tail(nullptr) {}

    /* 链表被销毁时，删除其中所有的定时器 */
    ~sort_timer_lst() {
        util_timer* tmp = head;
        while (tmp) {
            head = tmp->next;
            delete tmp;
            tmp = head;
        }
    }

    /* 将目标定时器 timer 添加到链表中 */
    void add_timer(util_timer* timer) {
        if (!timer) {
            return;
        }
        if (!head) {
            head = tail = timer;
            return;
        }
        /* 如果目标定时器的超时时间小于当前链表中所有定时器的超时时间，
           则把该定时器插入链表头部，作为链表新的头节点。 */
        if (timer->expire < head->expire) {
            timer->next = head;
            head->prev = timer;
            head = timer;
            return;
        }
        add_timer(timer, head);
    }

    /* 当某个定时任务发生变化时，调整对应的定时器在链表中的位置 */
    void adjust_timer(util_timer* timer) {
        if (!timer) {
            return;
        }
        util_timer* tmp = timer->next;

        /* 如果被调整的目标定时器处在链表尾部，或者该定时器新的超时值
           仍然小于其下一个定时器的超时值，则不用调整 */
        if (!tmp || (timer->expire < tmp->expire)) {
            return;
        }

        /* 如果目标定时器是链表的头节点，则将该定时器从链表中取出并重新插入链表 */
        if (timer == head) {
            head = head->next;
            head->prev = nullptr;
            timer->next = nullptr;
            add_timer(timer, head);
        }
        /* 如果目标定时器不是链表的头节点，则将该定时器从链表中取出，然后插入到适当位置 */
        else {
            timer->prev->next = timer->next;
            timer->next->prev = timer->prev;
            add_timer(timer, timer->next);
        }
    }

    /* 将目标定时器 timer 从链表中删除 */
    void del_timer(util_timer* timer) {
        if (!timer) {
            return;
        }

        /* 链表中只有一个定时器，删除后置空头尾指针 */
        if ((timer == head) && (timer == tail)) {
            delete timer;
            head = nullptr;
            tail = nullptr;
            return;
        }

        /* 目标定时器是头节点 */
        if (timer == head) {
            head = head->next;
            head->prev = nullptr;
            delete timer;
            return;
        }

        /* 目标定时器是尾节点 */
        if (timer == tail) {
            tail = tail->prev;
            tail->next = nullptr;
            delete timer;
            return;
        }

        /* 目标定时器在链表中间 */
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        delete timer;
    }

    /* SIGALRM 信号每次被触发，在其信号处理函数（或者主函数）中执行一次 tick 函数，
       以处理链表上到期的任务 */
    void tick() {
        if (!head) {
            return;
        }
        printf("timer tick\n");

        time_t cur = time(nullptr); /* 获取系统当前时间 */
        util_timer* tmp = head;

        /* 从头结点开始依次处理每个定时器，直到遇到一个尚未到期的定时器 */
        while (tmp) {
            /* 如果定时器未到期，退出循环 */
            if (cur < tmp->expire) {
                break;
            }

            /* 调用定时器的回调函数，以执行定时任务 */
            tmp->cb_func(tmp->user_data);

            /* 执行完定时任务后，将其从链表中删除，并重置链表头结点 */
            head = tmp->next;
            if (head) {
                head->prev = nullptr;
            }
            delete tmp;
            tmp = head;
        }
    }

private:
    /* 辅助函数：将目标定时器 timer 添加到节点 lst_head 之后的部分链表中 */
    void add_timer(util_timer* timer, util_timer* lst_head) {
        util_timer* prev = lst_head;
        util_timer* tmp = prev->next;

        /* 遍历 lst_head 之后的链表，找到合适的位置插入 */
        while (tmp) {
            if (timer->expire < tmp->expire) {
                prev->next = timer;
                timer->next = tmp;
                tmp->prev = timer;
                timer->prev = prev;
                return;
            }
            prev = tmp;
            tmp = tmp->next;
        }

        /* 若未找到合适的位置，则插入链表尾部，并更新 tail 指针 */
        prev->next = timer;
        timer->prev = prev;
        timer->next = nullptr;
        tail = timer;
    }

private:
    util_timer* head;
    util_timer* tail;
};

#endif /* LIST_H */