/*
    修复ET触发却没有循环读取导致服务器卡死的问题；将read()从主线程中放入处理线程中
    加入了定时器类，用于处理超时连接的问题
    将server独立写成一个类，从main中分离出来
*/
#include "server/server.h"

int main(){
    server webServer;
    webServer.workLoop();
    return 0;
}

