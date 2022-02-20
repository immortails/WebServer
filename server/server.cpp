#include"server.h"


extern int addfd(int epollfd,int fd,bool one_shot);
extern int removefd(int epollfd,int fd);
extern int setnonblocking(int fd);
int server::pipefd[2];



void addsig(int sig,void(handler)(int),bool restart=true){
    struct sigaction sa;
    memset(&sa,'\0',sizeof(sa));
    sa.sa_handler=handler;
    if(restart){
        sa.sa_flags|=SA_RESTART;        //自动重启被信号中断的系统调用
    }
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig,&sa,NULL)!=-1);
}




void server::initThreadPool(int threadNumber,int maxRequests){

    //创建线程池
    try{
        pool=new threadpool<clientData>;
    }
    catch(...){
        std::cout<<"creat threadPool wrong"<<std::endl;
        return ;
    }
    
}

server::server(){
    init();
}

server::~server(){
    close(epollfd);
    close(listenfd);
    delete pool;
}

void server::initSocket(const char* ip,const char* port){
    int servPort =atoi(PORT);
    listenfd=socket(PF_INET,SOCK_STREAM,0);
    assert(listenfd>=0);
    sockaddr_in servAddr;
    servAddr.sin_family=AF_INET;
    inet_pton(AF_INET,ip,&servAddr.sin_addr);
    servAddr.sin_port=htons(servPort);
    int ret=bind(listenfd,(sockaddr*)&servAddr,sizeof(servAddr));
    assert(ret>=0);
    ret=listen(listenfd,5);
    assert(ret>=0);
    epollfd=epoll_create(5);
    assert(epollfd!=-1);
    addfd(epollfd,listenfd,false);
    //定时器的事件，统一事件源
    addsig(SIGALRM,sigHandler);
    pipe(pipefd);
    pipe(pipefdHttp);
    setnonblocking(pipefd[1]);
    setnonblocking(pipefdHttp[1]);
    addfd(epollfd,pipefd[0],false);
    addfd(epollfd,pipefdHttp[0],false);         //让httpConn告知已超时
    alarm(DELAY);    //定时
}

void server::init(){
    initThreadPool();
    initSocket();
    addsig(SIGPIPE,SIG_IGN);
    users=new clientData[MAX_FD];
    //预先创建好每一个httpConn
    for(int i=0;i<MAX_FD;i++){
        users[i].clientHttp=new httpConn;
    }
    assert(users);
    httpConn::mEpollfd=epollfd;

}  

void server::sigHandler(int sig){
    int saveErrno=errno;
    int msg=sig;
    write(pipefd[1],(char*)&msg,1);
    errno=saveErrno;
}

void server::timerHandler(){
    //就是定时调用堆中的心跳函数。
    timeHeap::getInstance().tick();
    alarm(DELAY);
}

void server::workLoop(){

    while(1){
        //users[0].clientHttp->closeConn();
        int num=epoll_wait(epollfd,events,MAX_EVENT_NUMBER,-1);
        //std::cout<<num<<std::endl;
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
                    //std::cout<<"connfd "<<connfd<<std::endl;
                    if(connfd<0) break; 
                    assert(httpConn::mUserCount<MAX_FD);
                    //添加定时器
                    time_t cur=time(nullptr);
                    if(users[connfd].clientTimer!=nullptr){                         //去掉之前被关掉连接上的定时器，用一个新的代替
                        users[connfd].clientTimer->userData=nullptr;
                        users[connfd].clientTimer=nullptr;
                    }
                    //std::cout<<cur<<cur+3*DELAY<<std::endl;
                    users[connfd].clientTimer=new timer(cur+3*DELAY);
                    users[connfd].clientTimer->userData=&users[connfd];
                    timeHeap::getInstance().addTimer(users[connfd].clientTimer);
                    //将http事件完成注册
                    users[connfd].clientHttp->init(users[connfd].addr,connfd,pipefdHttp[1]);
                    //std::cout<<"over"<<std::endl; 
                }
                //std::cout<<"over"<<std::endl;
            }else if(events[i].events & (EPOLLRDHUP|EPOLLHUP|EPOLLERR)){          //如果连接异常直接关闭
                //if(events[i].events & (EPOLLRDHUP)) std::cout<<"EPOLLRDHUP"<<std::endl;
                //if(events[i].events & (EPOLLHUP)) std::cout<<"EPOLLHUP"<<std::endl;
                //if(events[i].events & (EPOLLERR)) std::cout<<"EPOLLERR"<<std::endl;
                users[sockfd].clientHttp->closeConn();
            }else if(sockfd==pipefd[0] && (events[i].events & EPOLLIN)){
                //处理信号
                int sig;
                int ret=read(pipefd[0],signals,sizeof(signals));
                if(ret==-1){
                    continue;
                }else if(ret==0){
                    continue;
                }else{
                    for(int i=0;i<ret;i++){
                        switch(signals[i]){
                            case SIGALRM:{  //先延迟一下，信号的事件不是很紧急
                                timeout=true;
                                break;
                            }
                        }
                    }
                }
            }else if(events[i].events & EPOLLIN){
                timeHeap::getInstance().adjTimer(users[sockfd].clientTimer);
                pool->append(users+sockfd);

            }
            if(timeout){
                timerHandler();
                timeout=false;
            }
        }
    }
}           
