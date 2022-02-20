#ifndef HEAPTIMER_H
#define HEAPTIMER_H
/*采用小根堆的定时器*/
#include<iostream>
#include<vector>
#include<netinet/in.h>
#include<time.h>
#include "../config.h"
#include "../http/httpConn.h"
class timeHeap;
class timer;
using std::vector;


/*绑定socket与定时器以及http处理对象*/
struct clientData{
    sockaddr_in addr;
    int sockfd;
    char buf[BUFF_SIZE];
    httpConn* clientHttp; 
    timer* clientTimer;
};


class timer{
public:
    time_t expire;                  //定时器生效的绝对时间
    clientData* userData;           //用户数据
    bool closedConn =false;         // 关闭连接的标记
public:
    timer(int _expire){
        expire=_expire;
    }
};

class timeHeap{
private:
    static timeHeap* timeContain;
    vector<timer*> heap;
    int size;
private:   
    //初始化为一个cap大小的空堆，时间堆只有一个所以用单例模式
    timeHeap();
    ~timeHeap();

public:
    static timeHeap& getInstance(){
        if(timeContain==nullptr) timeContain=new timeHeap();
        return (*timeContain);
    }
    void addTimer(timer* _timer);
    void delTimer(timer* _timer);
    void adjTimer(timer* _timer);
    void popTimer();
    timer* top() const;
    void tick();
private:
    void heapify(int idx);
    void heapInsert(int idx);
};

#endif