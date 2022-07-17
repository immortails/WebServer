#ifndef CONFIG_H
#define CONFIG_H
#include "cJSON/cJSON.h"
#include <fstream>
#include <iostream>
#include <sstream>
struct conf {
    int THREAD_NUMBER=8;              //线程池的线程数
    int MAX_REQUESTS=10000;           //线程池最大支持的连接数
    std::string IP;            //服务器ip地址
    std::string PORT;               //服务器的port
    int MAX_FD=65536;                 //epoll文件描述符数
    int MAX_EVENT_NUMBER=10000;       //最多支持事件个数
    int DELAY=3;                     //连接超时时间，单位s
    conf(int _THREAD_NUMBER, int _MAX_REQUESTS, std::string _IP,
         std::string _PORT, int _MAX_FD, int _MAX_EVENT_NUMBER, int _DELAY):THREAD_NUMBER(_THREAD_NUMBER), 
                MAX_REQUESTS(_MAX_REQUESTS),IP(_IP), PORT(_PORT), MAX_FD(_MAX_FD), MAX_EVENT_NUMBER(_MAX_EVENT_NUMBER),DELAY(_DELAY) {}
    conf(){};
};
conf g_conf;

void config_init() {
    /* 读取json */
    std::string file = "server_conf.json";
    int fd = open(file.c_str(),O_RDWR);
	char buf[2048]={0};
	int ret = read(fd, buf, sizeof(buf));
	close(fd);
    cJSON *config = nullptr;
    cJSON *threadNumber, *maxRequests, *ip, *port, *maxFD, *maxEventNumber, *delay;
    config = cJSON_Parse(buf);
    threadNumber = cJSON_GetObjectItem(config, "THREAD_NUMBER");
    maxRequests = cJSON_GetObjectItem(config, "MAX_REQUESTS");
    ip = cJSON_GetObjectItem(config, "IP");
    port = cJSON_GetObjectItem(config, "PORT");
    maxFD = cJSON_GetObjectItem(config, "MAX_FD");
    maxEventNumber = cJSON_GetObjectItem(config, "MAX_EVENT_NUMBER");
    delay = cJSON_GetObjectItem(config, "DELAY");

    g_conf.THREAD_NUMBER = threadNumber->valueint;
    g_conf.MAX_REQUESTS = maxRequests->valueint;
    g_conf.MAX_EVENT_NUMBER = maxEventNumber->valueint;
    g_conf.MAX_FD = maxFD->valueint;
    g_conf.PORT = port->valuestring;
    g_conf.IP = ip->valuestring;
    g_conf.DELAY = delay->valueint;

    cJSON_Delete(config);
}


#endif
