#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H
#include<unistd.h>
#include<signal.h>
#include<sys/types.h>
#include<sys/epoll.h>
#include<fcntl.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<assert.h>
#include<sys/stat.h>
#include<string.h>
#include<pthread.h>
#include<stdio.h>
#include<stdlib.h>
#include<sys/mman.h>
#include<stdarg.h>
#include<errno.h>
#include<iostream>
#include<sys/uio.h>
#include "../locker/locker.h"
#include "../config.h"

/*定时器类*/
const int BUFF_SIZE=64;


class httpConn{
public:
static const int FILENAME_LEN=200;          //文件名长度
static const int READ_BUFFER_SIZE=2048;     //读缓冲区的大小
static const int WRITE_BUFFER_SIZE=1024;    //写缓冲区大小
enum METHOD {GET=0,POST,HEAD,PUT,DELETE,TRACE,OPTIONS,CONNECT,PATCH};       //HTTP请求方法，目前支持GET
//解析客户请求时，主状态机所处在的状态
enum CHECK_STATE {  CHECK_STATE_REQUESTLINE=0,
                    CHECK_STATE_HEADER,
                    CHECK_STATE_CONTENT };
/*HTTP请求的结果，NO_REQUEST就是还没有获得一个完整的请求，GET_REQUEST是指获得了一个完整的请求
                BAD_REQUEST就是获取了一个错误的请求
*/
enum HTTP_CODE {    NO_REQUEST,GET_REQUEST,BAD_REQUEST,                     
                    NO_RESOURCE,FORBIDDEN_REQUEST,FILE_REQUEST,
                    INTERNAL_ERROR,CLOSED_CONNECTION };
enum LINE_STATUS { LINE_OK=0,LINE_BAD,LINE_OPEN};                           //行读取状态

public:
    httpConn();
    ~httpConn();
public:
    void init(sockaddr_in addr,int sock,int _pipefd);                                                  //初始化新接受的连接
    void closeConn( bool real_close=true);                                  //关闭连接
    void process();                                                         //处理客户请求
    bool read();                                                            //非阻塞读
    int writeData();                                                        //非阻塞写
private:
    void init();                                                            //初始化连接
    HTTP_CODE processRead();                                               //解析HTTP请求
    bool processWrite(HTTP_CODE ret);                                      //填充HTTP应答

    /*下面是用来分析HTTP请求的*/
    HTTP_CODE parseRequestLine(char* text);
    HTTP_CODE parseHeaders(char* text);
    HTTP_CODE parseContent(char* text);
    HTTP_CODE doRequest();
    char* getLine() {return mReadBuf+mStartLine;}
    LINE_STATUS parseLine();

    /*下面被process_write调用分析HTTP应答*/
    void unmap();
    bool addResponse(const char* format, ...);
    bool addContent(const char* content);
    bool addStatusLine(int status,const char* title);
    bool addHeaders(int content_length);
    bool addContentLength(int content_length);
    bool addLinger();
    bool addBlankLine();
public:
    static int mEpollfd;                   //所有socket上的事件都被注册到同一个epoll内核事件上，所以用static
    static int mUserCount;                 //统计用户数量
    int mPipefd;                    //向server中回传任务执行完毕
    int mSockfd;                           //该HTTP连接的socket
private:
    sockaddr_in mAddress;                  //对方的socket地址
    //读缓冲区
    char mReadBuf[READ_BUFFER_SIZE];      
    int mReadIdx;                           //标识读缓冲中已读数据的最后一个字节的下一个位置
    int mCheckedIdx;                        //当前正在分析的字符在读缓冲区中的位置
    int mStartLine;                         //当前正在解析的行首位置 
    //写缓冲区
    char mWriteBuf[WRITE_BUFFER_SIZE];    
    int mWriteIdx;                          //写缓冲区中待发送的字节数

    CHECK_STATE mCheckState;                //主状态机所处的状态
    METHOD mMthod;                          //请求方法

    char mRealFile[FILENAME_LEN];           //客户请求的目标文件的完整路径，即docRoot+mUrl
    char* mUrl;                             //客户请求的目标文件的文件名
    char* mVersion;                         //HTTP协议的版本号
    char* mHost;                            //主机名
    int mContentLength;                     //请求消息体的长度
    bool mLinger;                           //HTTP请求是否要保证连接
    
    
    char* mFileAddress;                     //客户请求的目标文件被mmap到内存中的起始位置。
    struct stat mFileStat;                  //目标文件的状态
    
    /*采用writev来执行写操作*/
    struct iovec mIv[2];
    int mIvCnt;

};
#endif
