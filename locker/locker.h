#ifndef LOCKER_H
#define LOCKER_H
#include<exception>
#include<pthread.h>
#include<semaphore.h>

/*封装信号量的类*/
class sem{
public:
    //初始化信号量
    sem(){
        if(sem_init(&m_sem,0,0)!=0){
            throw std::exception();
        }
    }
    //销毁信号量
    ~sem(){
        sem_destroy(&m_sem);
    }
    //等待信号量
    bool wait(){
        return sem_wait(&m_sem)==0;
    }
    //增加信号量
    bool post(){
        return sem_post(&m_sem)==0;
    }
private:
    sem_t m_sem;

};

/*封装锁*/
class locker{

private:
    pthread_mutex_t m_mutex;
    
public:
    //创建并初始化一个锁
    locker(){
        if(pthread_mutex_init(&m_mutex,NULL)!=0){
            throw std::exception();
        }
    }

    //销毁互斥锁
    ~locker(){
        pthread_mutex_destroy(&m_mutex);
    }

    //获得锁
    bool lock(){
        return pthread_mutex_lock(&m_mutex)==0;
    }

    //释放锁
    bool unlock(){
        return pthread_mutex_unlock(&m_mutex)==0;
    }

};

/*封装条件变量*/
class cond{

private:
    pthread_mutex_t m_mutex;
    pthread_cond_t m_cond;
public:
    cond(){
        if(pthread_mutex_init(&m_mutex,NULL)!=0){
            throw std::exception();
        }
        if(pthread_cond_init(&m_cond,NULL)!=0){
            //如果这里出了问题，还应该释放之前的锁
            pthread_mutex_destroy(&m_mutex);
            throw std::exception();
        }
    }

    ~cond(){
        pthread_mutex_destroy(&m_mutex);
        pthread_cond_destroy(&m_cond);
    }

    bool wait(){
        pthread_mutex_lock(&m_mutex);
        int ret=pthread_cond_wait(&m_cond,&m_mutex);
        pthread_mutex_unlock(&m_mutex);
        return ret== 0;
    }

    bool signal(){
        return pthread_cond_signal(&m_cond)==0;
    }
};
#endif