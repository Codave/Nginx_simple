#ifndef __NGX_SOCKET_H__
#define __NGX_SOCKET_H__

#include<vector>
#include<sys/epoll.h> //epoll
#include<sys/socket.h>

//一些宏定义
#define NGX_LISTEN_BACKLOG 511  //已完成链接队列大小
#define NGX_MAX_EVENTS     512  //epoll_wait一次最多接受这么多个事件

typedef struct ngx_listening_s       ngx_listening_t, *lpngx_listening_t;
typedef struct ngx_connection_s      ngx_connection_t, *lpngx_connection_t;
typedef class  CSocket               Csocket;

typedef void (CSocket::*ngx_event_handler_pt)(lpngx_connection_t c);    //定义成员函数指针

//一些专用结构定义放在这里，暂时不考虑放ngx_global.h里了
struct ngx_listening_s  //和监听有关的结构
{
    int     port;                       //监听的端口号
    int     fd;                         //套接字句柄socket
    lpngx_connection_t    connection;   //连接池的一个连接
};

//(1)该结构表示一个TCP连接【客户端主动发起的、Nginx服务器被动接受的TCP连接】
struct ngx_connection_s
{
    int                         fd;             //套接字句柄
    lpngx_listening_t           listening;      //如果这个链接被分配给了一个监听套接字，那么这个里边就指向监听套接字对应的那个lpngx_listening_t的内存首地址

    unsigned                    instance:1;
    uint64_t                    iCurrsequence;  //在一定程度上检测错包废包
    struct sockaddr             s_sockaddr;     //保存对方地址信息用的

    uint8_t                     w_ready;        //写准备好标记

    ngx_event_handler_pt        rhandler;       //读事件的相关处理方法
    ngx_event_handler_pt        whandler;       //写事件的相关处理方法

    lpngx_connection_t          data;           //next指针
};

//socket相关类
class CSocket
{
public:
    CSocket();
    virtual ~CSocket();

public:
    virtual bool Initialize();          //初始化函数

public:
    int ngx_epoll_init();   //epoll功能初始化 epoll_create
    int ngx_epoll_add_event(int fd,int readevent,int writeevent,uint32_t otherflag,uint32_t eventtype,lpngx_connection_t c);
    int ngx_epoll_process_events(int timer);    //epoll等待接受和处理事件 epoll_wait

private:
    void ReadConf();                    //专门用于读取各种配置项
    bool ngx_open_listening_sockets();  //监听必须的端口【支持多个端口】
    void ngx_close_listening_sockets(); //关闭监听套接字
    bool setnonblocking(int sockfd);    //设置非阻塞套接字

    //一些业务处理函数
    void ngx_event_accept(lpngx_connection_t oldc); //建立新连接
    void ngx_wait_request_handler(lpngx_connection_t c); //设置数据来时的读处理函数

    void ngx_close_accepted_connection(lpngx_connection_t c);//用户连入，accept4()时，得到的socket在处理中产生失败，则资源用这个函数释放
    
    //获取对端信息相关                                              
	size_t ngx_sock_ntop(struct sockaddr *sa,int port,u_char *text,size_t len);  //根据参数1给定的信息，获取地址端口字符串，返回这个字符串的长度

    //连接池 或 连接 相关
    lpngx_connection_t ngx_get_connection(int isock);   //从连接池中获取一个空闲连接
    void ngx_free_connection(lpngx_connection_t c);     //归还参数c所代表的连接到连接池中

private:
    int                              m_worker_connections;  //epoll连接的最大项数
    int                              m_ListenPortCount;     //所监听的端口数量
    int                              m_epollhandle;         //epoll_create返回的句柄

    //和连接池有关的
    lpngx_connection_t               m_pconnections;        //注意这里可是个指针，其实这是个连接池的首地址
    lpngx_connection_t               m_pfree_connections;   //空闲连接链表头

    int                              m_connection_n;        //当前进程中所有连接对象的总数【连接池大小】
    int                              m_free_connection_n;   //连接池中可用连接总数

    std::vector<lpngx_listening_t>   m_ListenSocketList;          //监听套接字队列

    struct epoll_event               m_events[NGX_MAX_EVENTS];    //用于在epoll_wait()中承载返回的所发生的的事件
};

#endif