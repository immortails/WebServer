/*
    2022.2.17 修复ET触发却没有循环读取导致服务器卡死的问题；将read()从主线程中放入处理线程中
*/




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
#include "locker/locker.h"
#include "threadPool/threadpool.h"
#include "http/httpConn.h"
#include "config.h"

#define MAX_FD 65536
#define MAX_EVENT_NUMBER 10000

extern int addfd(int epollfd,int fd,bool one_shot);
extern int removefd(int epollfd,int fd);

void addsig(int sig,void(handler)(int),bool restart=true){
    struct sigaction sa;
    memset(&sa,'\0',sizeof(sa));
    sa.sa_handler=handler;
    if(restart){
        sa.sa_flags|=SA_RESTART;
    }
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig,&sa,NULL)!=-1);
}

void showError(int connfd,const char* info){
    std::cout<<info<<std::endl;
    send(connfd,info,strlen(info),0);
    close(connfd);
}

int main(){
    //std::cout<<"server begin to run..."<<std::endl;
    const char* ip=IP;
    int port=atoi(PORT);
    //忽略SIGPIPE信号
    addsig(SIGPIPE,SIG_IGN);
    //创建线程池
    threadpool<httpConn>* pool=nullptr;
    try{
        pool=new threadpool<httpConn>;
    }
    catch(...){
        std::cout<<"creat threadPool wrong"<<std::endl;
        return 1;
    }
    //std::cout<<"created threadPool..."<<std::endl;
    httpConn* users=new httpConn[MAX_FD];
    assert(users);
    int userCnt=0;
    int listenfd=socket(PF_INET,SOCK_STREAM,0);
    assert(listenfd>=0);
    sockaddr_in servAddr;
    servAddr.sin_family=AF_INET;
    inet_pton(AF_INET,ip,&servAddr.sin_addr);
    servAddr.sin_port=htons(port);
    int ret=bind(listenfd,(sockaddr*)&servAddr,sizeof(servAddr));
    assert(ret>=0);
    ret=listen(listenfd,5);
    assert(ret>=0);
    //std::cout<<"create socket..."<<std::endl;
    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd=epoll_create(5);
    assert(epollfd!=-1);
    addfd(epollfd,listenfd,false);
    httpConn::mEpollfd=epollfd;
    //std::cout<<"create epoll..."<<std::endl;
    while(1){
        int num=epoll_wait(epollfd,events,MAX_EVENT_NUMBER,-1);
        if((num<0) && (errno!=EINTR)){
            std::cout<<"epoll failture"<<std::endl;
            break;
        }
        for(int i=0;i<num;i++){
            int sockfd=events[i].data.fd;
            if(sockfd==listenfd){                                                   //新的连接
                //std::cout<<"get new connection"<<std::endl;
                while(1){ //ET边沿触发
                    sockaddr_in clientAddr;
                    socklen_t clientAddrsz=sizeof(clientAddr);
                    int connfd=accept(listenfd,(sockaddr*)&clientAddr,&clientAddrsz);
                    if(connfd<0) break; 
                    assert(httpConn::mUserCount<MAX_FD);
                    users[connfd].init(connfd,clientAddr);   
                }
                           //在这里添加了sockfd与对应地址
            }else if(events[i].events & (EPOLLRDHUP|EPOLLRDHUP|EPOLLERR)){          //如果连接异常直接关闭
                users[sockfd].closeConn();
            }else if(events[i].events & EPOLLIN){
                //std::cout<<"have data"<<std::endl;
                //根据读的结果来决定将任务添加到线程池还是关闭连接
                //if(users[sockfd].read()){
                    //这步读竟然在主线程里实现，有点奇怪
                    //std::cout<<"read"<<std::endl;
                pool->append(users+sockfd);
                //}else{
                    //std::cout<<"wrong"<<std::endl;
                //    users[sockfd].closeConn();
                //}
            }else if(events[i].events & EPOLLOUT){
                //根据写的结果决定是否关闭连接，只是检查有没有写完
                if(!users[sockfd].write()){
                    users[sockfd].closeConn();
                }
            }
        }
    }
    close(epollfd);
    close(listenfd);
    delete pool;
    return 0;
}

