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
#include<errno.h>   //errno
// #include<sys/socket.h>
#include<sys/ioctl.h> //ioctl
#include<arpa/inet.h>
#include<pthread.h> 

#include "ngx_c_conf.h"
#include "ngx_macro.h"
#include "ngx_global.h"
#include "ngx_func.h"
#include "ngx_c_socket.h"
#include "ngx_c_memory.h"

//构造函数
CSocket::CSocket()
{
    //配置相关
    m_worker_connections = 1;    //epoll连接最大项数
    m_ListenPortCount = 1;       //监听一个端口

    //epoll相关
    m_epollhandle = -1;         //epoll返回的句柄
    m_pconnections = NULL;      //连接池【连接数组】  先给空
    m_pfree_connections = NULL; //连接池中空闲的连接池

    //一些和网络通讯有关的常用变量
    m_iLenPkgHeader = sizeof(COMM_PKG_HEADER);  //包头的sizeof值
    m_iLenMsgHeader = sizeof(STRUC_MSG_HEADER); //消息头的sizeof值

    m_iRecvMsgQueueCount = 0;

    //多线程相关
    pthread_mutex_init(&m_recvMessageQueueMutex,NULL);   //互斥量初始化

    return;
}

//析构函数
CSocket::~CSocket()
{
    //释放必须的内存
    //(1)监听端口相关内存的释放
    std::vector<lpngx_listening_t>::iterator pos;
    for(pos=m_ListenSocketList.begin();pos!=m_ListenSocketList.end();++pos)
    {
        delete (*pos);
    }
    m_ListenSocketList.clear();

    //(2)连接池相关的内容释放
    if(m_pconnections != NULL){
        delete[] m_pconnections;
    }

    //(3)接收消息队列中内容释放
    clearMsgRecvQueue();

    //(4)多线程相关
    pthread_mutex_destroy(&m_recvMessageQueueMutex);    //互斥量释放

    return;
}

//清理接收消息队列，注意这个函数的写法
void CSocket::clearMsgRecvQueue()
{
    char* sTmpMempoint;
    CMemory *p_memory = CMemory::GetInstance();

    while(!m_MsgRecvQueue.empty())
    {
        sTmpMempoint = m_MsgRecvQueue.front();
        m_MsgRecvQueue.pop_front();
        p_memory->FreeMemory(sTmpMempoint);
    }
}


//初始化函数
//成功返回true，失败返回false
bool CSocket::Initialize()
{   
    ReadConf();
    bool reco = ngx_open_listening_sockets();
    return reco;
}

//专门用于读各种配置项
void CSocket::ReadConf()
{
    CConfig* p_config = CConfig::GetInstance();
    m_worker_connections = p_config->GetIntDefault("worker_connections",m_worker_connections);//epoll连接的最大项数
    m_ListenPortCount    = p_config->GetIntDefault("ListenPortCount",m_ListenPortCount);//取得要监听的端口数量
    return;
}

//监听端口【支持多个端口】
bool CSocket::ngx_open_listening_sockets()
{
    int                 isock;       //socket;
    struct sockaddr_in  serv_addr;   //服务器的地址结构
    int                 iport;       //端口
    char                strinfo[100];//临时字符串

    //初始化相关
    memset(&serv_addr,0,sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    //中途用到一些配置信息
    CConfig* p_config = CConfig::GetInstance();
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
        p_listensocketitem->fd = isock;
        ngx_log_error_core(NGX_LOG_INFO,0,"监听%d端口成功!",iport);
        m_ListenSocketList.push_back(p_listensocketitem);
    }
    if(m_ListenSocketList.size() <= 0)  //不可能一个端口都不监听吧
    {
        return false;
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

//(1)epoll功能初始化，子进程中进行，本函数被ngx_worker_process_init()所调用
int CSocket::ngx_epoll_init()
{
    //(1)创建一个epoll对象，创建了一个红黑树，还创建了一个双向链表
    m_epollhandle = epoll_create(m_worker_connections);
    if(m_epollhandle == -1){
        ngx_log_stderr(errno,"CSocekt::ngx_epoll_init()中epoll_create()失败.");
        exit(2); //这是致命问题了，直接退，资源由系统释放吧，这里不刻意释放了，比较麻烦
    }

    //(2)创建连接池【数组】，用于处理所有客户端的链接
    m_connection_n = m_worker_connections;//记录当前连接池中连接总数
    
    m_pconnections = new ngx_connection_t[m_connection_n];  //创建连接池数组

    int i = m_connection_n;     //连接池中连接数
    lpngx_connection_t next = NULL;
    lpngx_connection_t c = m_pconnections;  //连接池数组首地址

    do{
        i--;
        
        c[i].data = next;       //设置连接对象的next指针，注意第一次循环时next=NULL
        c[i].fd = -1;           //初始化连接,无socket和该连接池中的连接绑定
        c[i].instance = 1;      //失效标志位设置为1
        c[i].iCurrsequence = 0; //当前序号统一从0开始

        next = &c[i];           //next指针前移
    }while(i);
    m_pfree_connections = next;             //设置空闲连接链表头指针，因为现在next指向c[0],现在整个链表都是空闲的
    m_free_connection_n = m_connection_n;  //空闲连接链表长度，因为现在整个链表都是空的，这两个长度相等

    //(3)遍历所有监听socket【监听端口】
    std::vector<lpngx_listening_t>::iterator pos;
    for(pos=m_ListenSocketList.begin();pos!=m_ListenSocketList.end();++pos)
    {
        c = ngx_get_connection((*pos)->fd); //从连接池中获取一个空闲连接对象
        if(c==NULL)
        {
            ngx_log_stderr(errno,"CSocekt::ngx_epoll_init()中ngx_get_connection()失败.");
            exit(2); //这是致命问题了，直接退，资源由系统释放吧，这里不刻意释放了，比较麻烦
        }
        c->listening = (*pos);  //连接对象和监听对象关联，方便通过连接对象找监听对象
        (*pos)->connection = c; //监听对象和连接对象关联，方便通过监听对象找连接对象

        //对监听端口的读事件设置处理方法，因为监听端口是用来等对方连接的发送三路握手的，所以监听端口关心的就是读事件
        c->rhandler = &CSocket::ngx_event_accept;

        //往监听socket上增加事件，从而让监听端口履行职责
        if(ngx_epoll_add_event((*pos)->fd,
                                1,0,
                                0,
                                EPOLL_CTL_ADD,
                                c
                                )==-1)
        {
            exit(2);
        }
    }
    return 1;
}





//epoll增加事件
//返回值:成功返回1，失败返回-1
int CSocket::ngx_epoll_add_event(int fd,                         //fd:句柄，一个socket
                                int readevent,int writeevent,    //readevent：表示是否是个读事件，0是，1不是  writeevent：表示是否是个写事件，0是，1不是
                                uint32_t otherflag,              // ET:EPOLLET  LT:0
                                uint32_t eventtype,              //eventtype：事件类型  ，一般就是用系统的枚举值，增加，删除，修改等;
                                lpngx_connection_t c             //c：对应的连接池中的连接的指针
                                )
{
    struct epoll_event ev;
    memset(&ev,0,sizeof(ev));

    if(readevent==1)
    {
        //读事件
        ev.events = EPOLLIN|EPOLLRDHUP;
    }
    else
    {
        //其他事件待处理
    }

    if(otherflag!=0)
    {
        ev.events |= otherflag;         
    }

    ev.data.ptr=(void*)((uintptr_t)c | c->instance);

    if(epoll_ctl(m_epollhandle,eventtype,fd,&ev)==-1)
    {
        ngx_log_stderr(errno,"CSocket::ngx_epoll_add_event()中epoll_ctl(%d,%d,%d,%u,%u)失败.",fd,readevent,writeevent,otherflag,eventtype);
        return -1;
    }
    return 1;
}



//开始获取发生的事件消息
int CSocket::ngx_epoll_process_events(int timer)
{
    int events = epoll_wait(m_epollhandle,m_events,NGX_MAX_EVENTS,timer);

    if(events == -1)
    {
        if(errno == EINTR)
        {
            //信号所致，直接返回
            ngx_log_error_core(NGX_LOG_INFO,errno,"CSocekt::ngx_epoll_process_events()中epoll_wait()失败!"); 
            return 1;  //正常返回
        }
        else
        {   
            //有问题
            ngx_log_error_core(NGX_LOG_INFO,errno,"CSocekt::ngx_epoll_process_events()中epoll_wait()失败!"); 
            return 0;  //非正常返回
        }
    }

    if(events == 0) //超时，但没事件来
    {
        if(timer!=-1)
        {
            //要求epoll_wait阻塞一定的时间而不是一直阻塞，这属于阻塞到时间了，则正常返回
            return 1;   //正常返回
        }
        //无线等待【所以不存在超时】，但却没返回任何事件，这不正常有问题
        ngx_log_error_core(NGX_LOG_ALERT,0,"CSocekt::ngx_epoll_process_events()中epoll_wait()没超时却没返回任何事件!"); 
        return 0; //非正常返回
    }

    //有事件到了
    lpngx_connection_t c;
    uintptr_t       instance;
    uint32_t        revents;
    for(int i=0;i<events;i++)
    {
        c = (lpngx_connection_t)(m_events[i].data.ptr);
        instance = (uintptr_t)c & 1;
        c = (lpngx_connection_t)((uintptr_t)c & (uintptr_t) ~1);

        //
        if(c->fd == -1)
        {
            ngx_log_error_core(NGX_LOG_DEBUG,0,"CSocekt::ngx_epoll_process_events()中遇到了fd=-1的过期事件:%p.",c); 
            continue; //这种事件就不处理即可
        }

        if(c->instance != instance)
        {
            ngx_log_error_core(NGX_LOG_DEBUG,0,"CSocekt::ngx_epoll_process_events()中遇到了instance值改变的过期事件:%p.",c); 
            continue; //这种事件就不处理即可
        }

        //正常处理事件
        revents = m_events[i].events;   //取出事件类型
        if(revents & (EPOLLERR|EPOLLHUP))//如果发生了错误或者客户端断连
        {
            //这加上读写标记，方便后续代码处理
            revents |= EPOLLIN|EPOLLOUT;
        }
        if(revents & EPOLLIN)   //如果是读事件
        {
            //一个客户端新连入
            (this->*(c->rhandler))(c);//注意括号的运用来正确设置优先级，防止编译出错；
                                    //如果新连接进入，这里执行的应该是CSocekt::ngx_event_accept(c)】            
                                    //如果是已经连入，发送数据到这里，则这里执行的应该是 CSocekt::ngx_wait_request_handler
        }
        if(revents & EPOLLOUT)  //如果是写事件
        {
            //待扩展
            ngx_log_stderr(errno,"111111111111111111111111111111.");
        }
    }
    return 1;
}