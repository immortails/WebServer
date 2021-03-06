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
#include "../timer/heapTimer.h"



class server{
private:
    int idx = 0;
    int listenfd;
    int mainEpollfd;
    int subEpollfd[3];
    threadpool<clientData>* pool=nullptr;
    epoll_event* events;
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
    void initThreadPool();          //初始化线程池
    void initSocket();              //初始化连接
    void initEpollfd(int* epollfd);
    void init();                    //初始化
    void workLoop();                
    void subRactor(int* curEpollfd);    //负责处理事件
    void mainRactor(int* curEpollfd);   //只负责接收新连接
};

#endif