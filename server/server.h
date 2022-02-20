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
#include<errno.h>
#include<unordered_set>
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
    //下面都是定时器的操作
    char signals[1024];
    char closefdFromHC[1024];
    bool timeout=false;
    void static sigHandler(int sig);        //定时器回调函数要设成静态的，不然传不进去参数
    void static timerHandler();
public:
static int pipefd[2];
    int pipefdHttp[2];
    server();
    ~server();
    void initThreadPool(int threadNumber=THREAD_NUMBER,int maxRequests=MAX_REQUESTS);          //初始化线程池
    void initSocket(const char* ip=IP,const char* port=PORT);              //初始化连接
    void init();                    //初始化
    void workLoop();                //主循环
};

#endif