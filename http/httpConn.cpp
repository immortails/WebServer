#include "httpConn.h"

//定义HTTP响应的一些状态信息
const char* ok_200_title="OK";
const char* error_400_title="Bad Request";
const char* error_400_form = "Your request hash bad syntax or is inherently impossible to satisfy.\n";
const char* error_403_title="Forbidden";
const char* error_403_form="You don't have permission to get file from this server.\n";
const char* error_404_title="Not Found";
const char* error_404_form="The requested file was not found on this server.\n";
const char* error_500_title="Internal Error";
const char* error_500_form="There was an unusual problem serving the requested file.\n";

//网站的根目录
const char* docRoot="/var/www/html";

int setnonblocking(int fd){                         //修改文件描述符非阻塞
    int old_option=fcntl(fd,F_GETFL);
    int new_option=old_option|O_NONBLOCK;
    fcntl(fd,F_SETFL,new_option);
    return old_option;
}

void addfd(int epollfd,int fd,bool one_shot){       //添加epoll事件
    epoll_event event;
    event.data.fd=fd;
    event.events=EPOLLIN|EPOLLET|EPOLLRDHUP;
    if(one_shot){
        event.events|=EPOLLONESHOT;
    }
    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);
    setnonblocking(fd);
}

void removefd(int epollfd,int fd){
    epoll_ctl(epollfd,EPOLL_CTL_DEL,fd,0);
    close(fd);
}

void modfd(int epollfd,int fd,int ev){
    epoll_event event;
    event.data.fd=fd;
    event.events=ev|EPOLLET|EPOLLONESHOT|EPOLLRDHUP;
    epoll_ctl(epollfd,EPOLL_CTL_MOD,fd,&event);
}

int httpConn::mUserCount=0;
int httpConn::mEpollfd=-1;

void httpConn::closeConn(bool realClose){
    if(realClose&&(mSockfd!=-1)){
        removefd(mEpollfd,mSockfd);
        mSockfd=-1;
        mUserCount--;
    }
}

void httpConn::init(int sockfd,const sockaddr_in& addr){
    mSockfd=sockfd;
    mAddress=addr;
    //关掉TIME_WAIT的，真实代码应该删除下面两行
    int reuse=1;
    setsockopt(mSockfd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));
    addfd(mEpollfd,sockfd,true);
    mUserCount++;
    init();
}

void httpConn::init(){
    mCheckState=CHECK_STATE_REQUESTLINE;
    mLinger=false;

    mMthod=GET;
    mUrl=0;
    mVersion=0;
    mContentLength=0;
    mHost=0;
    mStartLine=0;
    mCheckedIdx=0;
    mReadIdx=0;
    mWriteIdx=0;
    memset(mReadBuf,'\0',READ_BUFFER_SIZE);
    memset(mWriteBuf,'\0',WRITE_BUFFER_SIZE);
    memset(mRealFile,'\0',FILENAME_LEN);
}

httpConn::LINE_STATUS httpConn::parseLine(){                        //去掉一行中的\r\n
    char temp;      
    for(;mCheckedIdx<mReadIdx;mCheckedIdx++){
        temp=mReadBuf[mCheckedIdx];
        if(temp=='\r'){
            if((mCheckedIdx+1)==mReadIdx){
                return LINE_OPEN;
            }else if(mReadBuf[mCheckedIdx+1]=='\n'){                //去掉\r\n 
                mReadBuf[mCheckedIdx++]='\0';
                mReadBuf[mCheckedIdx++]='\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }else if(temp=='\n'){
            if((mCheckedIdx>1) && mReadBuf[mCheckedIdx-1]=='\r'){   //也是去掉\r\n
                mReadBuf[mCheckedIdx-1]='\0';
                mReadBuf[mCheckedIdx++]='\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}


bool httpConn::read(){                //循环读取数据直到无数据可读
    int flag=true;
    if(mReadIdx >= READ_BUFFER_SIZE){
        flag=false;
    }
    if(flag){
        int bytesRead=0;
        while(1){
            bytesRead=recv(mSockfd,mReadBuf+mReadIdx,READ_BUFFER_SIZE-mReadIdx,0);
            if(bytesRead==-1){
                if(errno==EAGAIN||errno==EWOULDBLOCK){
                    break;
                }
                flag= false;
                break;
            }else if(bytesRead==0){
                flag= false;
                break;
            }
            mReadIdx+=bytesRead;
        }
    }
    if(!flag) closeConn();
    return flag;
}

//解析请求行，获得请求方法，目标URL，以及HTTP版本号
httpConn::HTTP_CODE httpConn::parseRequestLine(char* text){
    mUrl=strpbrk(text," \t");
    if(!mUrl){
        return BAD_REQUEST;
    }
    *mUrl++='\0';

    char* method=text;
    if(strcasecmp(method,"GET")==0){        //暂时仅支持GET
        mMthod=GET;
    }else{
        //printf("get wrong\n");
        return BAD_REQUEST;
    }

    mUrl+=strspn(mUrl," \t");                //去掉Tab与空格
    mVersion=strpbrk(mUrl," \t");           
    if(!mVersion){
        //printf("version wrong \n");
        return BAD_REQUEST;
    }
    *mVersion++='\0';
    mVersion+=strspn(mVersion," \t");
    if(strcasecmp(mVersion,"HTTP/1.1")!=0){
        return BAD_REQUEST;
    }
    if(strncasecmp(mUrl,"http://",7)==0){
        mUrl+=7;
        mUrl=strchr(mUrl,'/');         //去除掉前面的http
    }
    if(!mUrl || mUrl[0]!='/'){
        return BAD_REQUEST;
    }
    mCheckState=CHECK_STATE_HEADER;
    return NO_REQUEST;
}

//解析http头部
httpConn::HTTP_CODE httpConn::parseHeaders(char* text){
    if(text[0]=='\0'){  //遇到空，说明解析完毕
        //如果http请求有消息体，则还需要读取mContentLength的消息体，状态机转移到CHECK_STATE_CONTENT的状态
        if(text[0]=='\0'){
            mCheckState=CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST; //否则得到了一个完整的http请求
    }
        
    //处理connection 头部字段
    else if(strncasecmp(text,"Connection:",11)==0){
        text+=11;
        text+=strspn(text,"\t");
        if(strcasecmp(text,"keep-alive")==0){
            mLinger=true;
        }
    }
    //处理Content-Length 头部字段
    else if(strncasecmp(text,"Content-Length:",15)==0){
        text+=15;
        text+=strspn(text,"\t");
    }

    //处理host头部字段
    else if(strncasecmp(text,"host:",5)==0){
        text+=5;
        text+=strspn(text,"\t");
        mHost=text;
    }

    else{
        //std::cout<<"oop! unknow header"<<text<<std::endl;
    }
    return NO_REQUEST;
}

//只是判断是否读入了，没有真正解析消息体
httpConn::HTTP_CODE httpConn::parseContent(char* text){
    if(mReadIdx>=(mContentLength+mCheckedIdx)){
        text[mContentLength]='\0';
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

httpConn::HTTP_CODE httpConn::processRead(){
    LINE_STATUS lineStatus=LINE_OK;
    HTTP_CODE ret=NO_REQUEST;
    char* text=0;
    while(((mCheckState==CHECK_STATE_CONTENT)&&(lineStatus==LINE_OK)) || ((lineStatus=parseLine())==LINE_OK)){
        text=getLine();
        mStartLine=mCheckedIdx;
        //std::cout<<"got 1 http line:"<<text<<std::endl;
        switch(mCheckState){
            case CHECK_STATE_REQUESTLINE:{
                //std::cout<<"this is requestLine"<<std::endl;
                ret=parseRequestLine(text);     //处理请求行
                if(ret==BAD_REQUEST){
                    //std::cout<<"BAD_REQUEST"<<std::endl;
                    return BAD_REQUEST;
                }
                break;
            }
            case CHECK_STATE_HEADER:{
                //std::cout<<"this is header"<<std::endl;
                ret=parseHeaders(text);         //处理请求头部字段
                if(ret==BAD_REQUEST){
                    return BAD_REQUEST;
                }else if(ret==GET_REQUEST){
                    return doRequest();
                }
                break;
            }
            case CHECK_STATE_CONTENT:{
                //std::cout<<"this is content"<<std::endl;
                ret=parseContent(text);
                if(ret==GET_REQUEST){
                    return doRequest();
                }
                lineStatus=LINE_OPEN;
                break;
            }
            default:{
                //std::cout<<"this is wrong"<<std::endl;
                return INTERNAL_ERROR;
            }
        }
    }
    return NO_REQUEST;
}

/*当得到了一个完整，正确的HTTP请求时，我们就分析目标的文件属性，如果存在，就用mmap映射到相应内存地址mFileAddress中，并告诉调用者获取文件成功*/
httpConn::HTTP_CODE httpConn::doRequest(){
    stpcpy(mRealFile,docRoot);
    int len=strlen(docRoot);
    strncpy(mRealFile+len,mUrl,FILENAME_LEN-len-1);     //填充文件名
    if(stat(mRealFile,&mFileStat)<0){                   //获取文件状态
        return NO_RESOURCE;
    }

    if(!(mFileStat.st_mode&S_IROTH)){                   //文件不可读   
        return FORBIDDEN_REQUEST;
    }
    if(S_ISDIR(mFileStat.st_mode)){                     //文件是个目录，http请求有问题
        return BAD_REQUEST;
    }
    int fd=open(mRealFile,O_RDONLY);
    mFileAddress=(char*)mmap(0,mFileStat.st_size,PROT_READ,MAP_PRIVATE,fd,0);   //开始地址是0，大小是文件的大小，访问模式只读，文件描述符是fd，偏移量是0
    close(fd);
    return FILE_REQUEST;
}

//对处理完毕的文件执行unmap操作
void httpConn::unmap(){
    if(mFileAddress){
        munmap(mFileAddress,mFileStat.st_size);
        mFileAddress=0;
    }
}

//写HTTP响应
bool httpConn::write(){
    int temp=0;
    int bytesHaveSend=0;
    int bytesToSend=mWriteIdx;
    if(bytesToSend==0){
        modfd(mEpollfd,mSockfd,EPOLLIN);
        init();                             //写完后将其初始化，清空之前的
        return true;
    }

    while(1){
        temp=writev(mSockfd,mIv,mIvCnt);
        if(temp<=-1){
            //如果TCP写缓冲没有空间，则等待下一轮EPOLLOUT事件，虽然此时服务器无法立刻收到同一个用户的下一个请求，但是这样保证了完整性。
            if(errno==EAGAIN){              //此时读完了
                modfd(mEpollfd,mSockfd,EPOLLOUT);   
                return true;
            }
            unmap();
            return false;
        }
        bytesToSend-=temp;
        bytesHaveSend+=temp;
        if(bytesToSend<=bytesHaveSend){
            //发送HTTP相应成功，根据HTTP请求中的Connection字段决定是否立刻关闭连接
            unmap();
            if(mLinger){
                init();
                modfd(mEpollfd,mSockfd,EPOLLIN);
                return true;
            }else{
                modfd(mEpollfd,mSockfd,EPOLLIN);
                return false;
            }
        }
    }
}

//往写缓冲中写入待发送数据
bool httpConn::addResponse(const char* format, ...){
    if(mWriteIdx>=WRITE_BUFFER_SIZE){
        return false;
    }
    va_list argList;
    va_start(argList,format);
    int len=vsnprintf(mWriteBuf+mWriteIdx,WRITE_BUFFER_SIZE-1-mWriteIdx,format,argList);
    if(len>=(WRITE_BUFFER_SIZE-1-mWriteIdx)){
        return false;
    }
    mWriteIdx+=len;
    va_end(argList);
    return true;
}

bool httpConn::addStatusLine(int status,const char* title){
    return addResponse("%s,%d %s\r\n","HTTP/1.1",status,title);
}

bool httpConn::addHeaders(int contentLen){
    bool res=true;
    res&=addContentLength(contentLen);
    res&=addLinger();
    res&=addBlankLine();
    return res;
}

bool httpConn::addContentLength(int contentLen){
    return addResponse("Content-Length: %d\r\n",contentLen);
}

bool httpConn::addLinger(){
    return addResponse("Connection: %s\r\n",mLinger==true ? "keep-alive":"close");
}

bool httpConn::addBlankLine(){
    return addResponse("%s","\r\n");
}

bool httpConn::addContent(const char* content){
    return addResponse("%s",content);
}

bool httpConn::processWrite(HTTP_CODE ret){
    switch(ret){
        case INTERNAL_ERROR:{
            addStatusLine(500,error_500_title);
            addHeaders(strlen(error_500_form));
            if(!addContent(error_500_form)){
                return false;
            }
            break;
        }
        case BAD_REQUEST:{
            addStatusLine(400,error_400_title);
            addHeaders(strlen(error_400_form));
            if(!addContent(error_400_form)){
                return false;
            }
            break;
        }
        case NO_RESOURCE:{
            addStatusLine(404,error_400_title);
            addHeaders(strlen(error_404_form));
            if(!addContent(error_404_form)){
                return false;
            }
            break;
        }
        case FORBIDDEN_REQUEST:{
            addStatusLine(403,error_403_title);
            addHeaders(strlen(error_403_form));
            if(!addContent(error_403_form)){
                return false;
            }
            break;
        }
        case FILE_REQUEST:{         //这个比较特殊，因为要加响应文件，所以直接写了直接返回，不用在最后添加到iov里
            addStatusLine(200,ok_200_title);
            if(mFileStat.st_size!=0){
                addHeaders(mFileStat.st_size);
                //先写响应头部
                mIv[0].iov_base=mWriteBuf;
                mIv[0].iov_len=mWriteIdx;
                //再写响应的文件
                mIv[1].iov_base=mFileAddress;
                mIv[1].iov_len=mFileStat.st_size;
                mIvCnt=2;
                return true;
            }else{
                const char* ok_string="<html><body></body></html>";
                addHeaders(strlen(ok_string));
                if(!addContent(ok_string)){
                    return false;
                }
                break;
            }
        }
        default:{
            return false;
        }
    }

    mIv[0].iov_base=mWriteBuf;
    mIv[1].iov_len=mWriteIdx;
    mIvCnt=1;
    return true;
}

//由线程池调用，这是入口函数
void httpConn::process(){
    HTTP_CODE readRet=processRead();
    if(readRet==NO_REQUEST){                //还没有接收完，需要继续接收
        modfd(mEpollfd,mSockfd,EPOLLIN);
        return ;
    }
    bool writeRet =processWrite(readRet);
    if(!writeRet){
        closeConn();
    }
    modfd(mEpollfd,mSockfd,EPOLLOUT);
}