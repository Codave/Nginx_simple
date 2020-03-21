//和网络有关的函数放在这里

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<stdint.h>  //uintptr_t
#include<stdarg.h>  //va_start....
#include<unistd.h>  //STDERR_FILENO等
#include<sys/time.h> //gettimeofday
#include<time.h>    //localtime_r
#include<fcntl.h>   //open
#include<errno.h>
#include<sys/socket.h>
#include<sys/ioctl.h>
#include<arpa/inet.h>

#include "ngx_c_conf.h"
#include "ngx_macro.h"
#include "ngx_global.h"
#include "ngx_func.h"
#include "ngx_c_socket.h"

//构造函数
CSocket::CSocket()
{
    m_ListenPortCount = 1;  //监听一个端口
    return;
}

//析构函数
CSocket::~CSocket()
{
    //释放必须的内存
    std::vector<lpngx_listening_t>::iterator pos;
    for(pos=m_ListenSocketList.begin();pos!=m_ListenSocketList.end();++pos)
    {
        delete (*pos);
    }
    m_ListenSocketList.clear();
    return;
}

//初始化函数
//成功返回true，失败返回false
bool CSocket::Initialize()
{
    bool reco = ngx_open_listening_sockets();
    return reco;
}

//监听端口【支持多个端口】
bool CSocket::ngx_open_listening_sockets()
{
    CConfig* p_config = CConfig::GetInstance();
    m_ListenPortCount = p_config->GetIntDefault("ListenPortCount",m_ListenPortCount);

    int                 isock;       //socket;
    struct sockaddr_in  serv_addr;   //服务器的地址结构
    int                 iport;       //端口
    char                strinfo[100];//临时字符串

    //初始化相关
    memset(&serv_addr,0,sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    for(int i=0;i<m_ListenPortCount;i++)
    {
        isock = socket(AF_INET,SOCK_STREAM,0);
        if(isock == -1)
        {
            ngx_log_stderr(errno,"CSocket::Initialize()中socket()失败,i=%d",i);
            return false;
        }

        int reuseaddr = 1;
        if(setsockopt(isock,SOL_SOCKET,SO_REUSEADDR,(const void*)&reuseaddr,sizeof(reuseaddr))==-1)
        {
            ngx_log_stderr(errno,"CSocekt::Initialize()中setsockopt(SO_REUSEADDR)失败,i=%d.",i);
            close(isock); //无需理会是否正常执行了                                                  
            return false;
        }

        //设置该socket为非阻塞
        if(setnonblocking(isock)==false)
        {
            ngx_log_stderr(errno,"CSocekt::Initialize()中setnonblocking()失败,i=%d.",i);
            close(isock);
            return false;
        }

        //设置本服务器监听的地址和端口，这样客户端才能连接到该地址和端口并发送数据
        strinfo[0]=0;
        sprintf(strinfo,"ListenPort%d",i);
        iport = p_config->GetIntDefault(strinfo,10000);
        serv_addr.sin_port = htons((in_port_t)iport);

        //绑定服务器地址结构
        if(bind(isock,(struct sockaddr*)&serv_addr,sizeof(serv_addr)) == -1)
        {
            ngx_log_stderr(errno,"CSocekt::Initialize()中bind()失败,i=%d.",i);
            close(isock);
            return false;
        }

        //开始监听
        if(listen(isock,NGX_LISTEN_BACKLOG)==-1)
        {
            ngx_log_stderr(errno,"CSocekt::Initialize()中listen()失败,i=%d.",i);
            close(isock);
            return false;
        }

        //放到列表里来
        lpngx_listening_t p_listensocketitem = new ngx_listening_t;
        memset(p_listensocketitem,0,sizeof(ngx_listening_t));
        p_listensocketitem->port = iport;
        p_listensocketitem->port = isock;
        ngx_log_error_core(NGX_LOG_INFO,0,"监听%d端口成功!",iport);
        m_ListenSocketList.push_back(p_listensocketitem);
    }
    return true;
}

//设置socket连接为非阻塞
bool CSocket::setnonblocking(int sockfd)
{
    int nb=1;
    if(ioctl(sockfd,FIONBIO,&nb)==-1) //FIONBIO：设置/清除非阻塞I/O标记：0：清除，1：设置
    {
        return false;
    }
    return true;
    
    //如下也是一种写法，跟上边这种写法其实是一样的，但上边的写法更简单
    /* 
    //fcntl:file control【文件控制】相关函数，执行各种描述符控制操作
    //参数1：所要设置的描述符，这里是套接字【也是描述符的一种】
    int opts = fcntl(sockfd, F_GETFL);  //用F_GETFL先获取描述符的一些标志信息
    if(opts < 0) 
    {
        ngx_log_stderr(errno,"CSocekt::setnonblocking()中fcntl(F_GETFL)失败.");
        return false;
    }
    opts |= O_NONBLOCK; //把非阻塞标记加到原来的标记上，标记这是个非阻塞套接字【如何关闭非阻塞呢？opts &= ~O_NONBLOCK,然后再F_SETFL一下即可】
    if(fcntl(sockfd, F_SETFL, opts) < 0) 
    {
        ngx_log_stderr(errno,"CSocekt::setnonblocking()中fcntl(F_SETFL)失败.");
        return false;
    }
    return true;
    */
}

//关闭socket，什么时候用，我们现在先不确定，先把这个函数预备在这里
void CSocket::ngx_close_listening_sockets()
{
    for(int i = 0; i < m_ListenPortCount; i++) //要关闭这么多个监听端口
    {  
        //ngx_log_stderr(0,"端口是%d,socketid是%d.",m_ListenSocketList[i]->port,m_ListenSocketList[i]->fd);
        close(m_ListenSocketList[i]->fd);
        ngx_log_error_core(NGX_LOG_INFO,0,"关闭监听端口%d!",m_ListenSocketList[i]->port); //显示一些信息到日志中
    }//end for(int i = 0; i < m_ListenPortCount; i++)
    return;
}