#include"server.h"


extern int addfd(int epollfd,int fd,bool one_shot);
extern int removefd(int epollfd,int fd);
extern int setnonblocking(int fd);


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
    setnonblocking(pipefd[1]);
    addfd(epollfd,pipefd[0],false);
    alarm(TIMESLOT);    //定时
}

void server::init(){
    initThreadPool();
    initSocket();
    addsig(SIGPIPE,SIG_IGN);
    users=new clientData[MAX_FD];
    timerContain=new timeHeap(100);
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
    send(pipefd[1],(char*)msg,1,0);
    errno=saveErrno;
}

void server::timerHandler(){
    //就是定时调用堆中的心跳函数。
    timerContain->tick();
    alarm(TIMESLOT);
}

void server::cbFunc(clientData* userData){
    removefd(epollfd,userData->sockfd);
}

void server::workLoop(){

    while(1){
        //users[0].clientHttp->closeConn();
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
                    //std::cout<<connfd<<std::endl;
                    if(connfd<0) break; 
                    assert(httpConn::mUserCount<MAX_FD);
                    //添加定时器
                    time_t cur=time(NULL);
                    users[connfd].clientTimer=new timer(cur+DELAY);
                    timerContain->addTimer(users[connfd].clientTimer);
                    //将http事件完成注册
                    users[connfd].clientHttp->init(connfd,clientAddr);              
                }
            }else if(events[i].events & (EPOLLRDHUP|EPOLLRDHUP|EPOLLERR)){          //如果连接异常直接关闭
                users[sockfd].clientHttp->closeConn();
            }else if(events[i].events & EPOLLIN){
                pool->append(users+sockfd);
            }else if(events[i].events & EPOLLOUT){
                //如果TCP写缓冲有了写空间，就把相应的没写完的东西写完，
                //之所以放到主线程里是因为没必要让工作线程一直等待。
                if(!users[sockfd].clientHttp->write()){
                    users[sockfd].clientHttp->closeConn();
                }
            }
        }
    }
}           
