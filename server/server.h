#ifndef SERVER_H
#define SERVER_H 

#include<iostream>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<stdio.h>
#include<unistd.h>
#include<errno.h>
#include<string.h>
#include<fcntl.h>
#include<stdlib.h>
#include<cassert>
#include<sys/epoll.h>
#include<iostream>
#include "../locker/locker.h"
#include "../threadPool/threadpool.h"
#include "../http/httpConn.h"
#include "../config.h"
#include "../timer/heapTimer.h"

class server{
private:
    int listenfd;
    int epollfd;
    threadpool<clientData>* pool=nullptr;
    epoll_event events[MAX_EVENT_NUMBER];
    clientData* users;
public:
    server();
    ~server();
    void initThreadPool(int threadNumber=THREAD_NUMBER,int maxRequests=MAX_REQUESTS);          //初始化线程池
    void initSocket(const char* ip=IP,const char* port=PORT);              //初始化连接
    void init();                    //初始化
    void workLoop();                //主循环
};

#endif