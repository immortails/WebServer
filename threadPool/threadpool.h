#ifndef THREADPOOL_H
#define THREADPOOL_H

#include<list>
#include<exception>
#include<pthread.h>
#include "../locker/locker.h"
#include "../config.h"

/*线程池类，将它定义为模板类*/
template<typename T>
class threadpool{
public:
    /*参数thread_number是线程池中线程的数量，max_requests是请求队列中最多允许的处理的请求数量*/
    threadpool(int thread_number=THREAD_NUMBER,int max_requests=MAX_REQUESTS);
    ~threadpool();
    //往请求队列中添加任务
    bool append(T* request);
private:
    //工作线程中运行的函数，它不断从工作队列中取出任务并执行
    static void* worker(void* arg);
    void run();


    int mThreadNumber;        //线程池中的线程数
    int mMaxRequests;         //请求队列中最大允许的请求数
    pthread_t* mThreads;       //描述线程池的数组，其大小为m_thread_number
    std::list<T*> mWorkqueue;  //请求队列
    locker mQueuelocker;       //保护请求队列的锁
    sem mQueuestat;            //是否有任务需要处理
    bool mStop;                //是否结束线程
};

template<typename T>
threadpool<T>::threadpool(int threadNumber,int maxRequests):mThreadNumber(threadNumber),mMaxRequests(maxRequests),mStop(false),mThreads(nullptr){
 if((threadNumber<=0)||(maxRequests<=0)){
     throw std::exception();
 }
 mThreads=new pthread_t[mThreadNumber];
 if(!mThreads){
     throw std::exception();
 }
 for(int i=0;i<threadNumber;i++) {
     if(pthread_create(mThreads+i,NULL,worker,this)!=0){
         delete [] mThreads;
         throw std::exception();
        }
    }  
}


template<typename T>
threadpool<T>::~threadpool(){
    delete [] mThreads;
    mStop=true;
}

template< typename T>
bool threadpool<T>::append(T* request){
    mQueuelocker.lock();
    if(mWorkqueue.size()>mMaxRequests){
        mQueuelocker.unlock();
        return false;
    }
    mWorkqueue.push_back(request);
    mQueuelocker.unlock();
    //std::cout<<"get request"<<std::endl;
    mQueuestat.post();
    return true;
}

template<typename T>
void* threadpool<T>::worker(void* arg){
    threadpool* pool=(threadpool*) arg;
    //printf("create the thread\n");
    pool->run();
    return pool;
}

template<typename T>
void threadpool<T>::run(){
    //std::cout<<"run"<<std::endl;
    while(!mStop){
        mQueuestat.wait();
        mQueuelocker.lock();
        if(mWorkqueue.empty()){
            mQueuelocker.unlock();
            continue;
        }
        T* request=mWorkqueue.front();
        mWorkqueue.pop_front();
        mQueuelocker.unlock();
        if(!request){
            continue;
        }
        request->clientHttp->read();
        //std::cout<<"get a task"<<std::endl;
        request->clientHttp->process();
    }
}
#endif
