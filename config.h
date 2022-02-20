#ifndef CONFIG_H
#define CONFIG_H
    const int THREAD_NUMBER=8;              //线程池的线程数
    const int MAX_REQUESTS=10000;           //线程池最大支持的连接数
    const char IP[]="127.0.0.1";            //服务器ip地址
    const char PORT[]="1500";               //服务器的port

    const int MAX_FD=65536;                 //epoll文件描述符数
    const int MAX_EVENT_NUMBER=10000;       //最多支持事件个数

    const int DELAY=3;                     //连接超时时间，单位s


#endif
