//和网络 中 接受连接【accept】 有关的函数放这里

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>    //uintptr_t
#include <stdarg.h>    //va_start....
#include <unistd.h>    //STDERR_FILENO等
#include <sys/time.h>  //gettimeofday
#include <time.h>      //localtime_r
#include <fcntl.h>     //open
#include <errno.h>     //errno
//#include <sys/socket.h>
#include <sys/ioctl.h> //ioctl
#include <arpa/inet.h>

#include "ngx_c_conf.h"
#include "ngx_macro.h"
#include "ngx_global.h"
#include "ngx_func.h"
#include "ngx_c_socket.h"

//建立新连接专用函数，当新连接进入时，本函数会被ngx_epoll_process_events()所调用
//ngx_process_cycle.cxx [ ngx_worker_process_cycle() ] -> 
//ngx_event.cxx [ ngx_process_events_and_timers() ] ->  
//ngx_c_socket.cxx [ngx_epoll_process_events()]   
void CSocket::ngx_event_accept(lpngx_connection_t oldc)
{
    struct sockaddr             mysockaddr; //远端客户端的socket地址
    socklen_t                   socklen;
    int                         err;
    int                         level;
    int                         s;
    static int                  use_accept4 = 1;
    lpngx_connection_t          newc;   //代表连接池中的一个连接

    socklen = sizeof(mysockaddr);
    do{
        if(use_accept4)
        {
            s = accept4(oldc->fd,&mysockaddr,&socklen,SOCK_NONBLOCK);
        }
        else
        {
            s = accept(oldc->fd,&mysockaddr,&socklen);
        }

        if(s==-1)
        {
            err = errno;
            if(err==EAGAIN)
            {
                return;
            }
            level = NGX_LOG_ALERT;
            if(err==ECONNABORTED)
            {
                level = NGX_LOG_ERR;
            }
            else if (err == EMFILE || err == ENFILE)
            {
                level = NGX_LOG_CRIT;
            }
            ngx_log_error_core(level,errno,"CSocekt::ngx_event_accept()中accept4()失败!");

            if(use_accept4 && err == ENOSYS) //accept4()函数没实现，坑爹？
            {
                use_accept4 = 0;  //标记不使用accept4()函数，改用accept()函数
                continue;         //回去重新用accept()函数搞
            }

            if (err == ECONNABORTED)  //对方关闭套接字
            {
                //这个错误因为可以忽略，所以不用干啥
                //do nothing
            }
            
            if (err == EMFILE || err == ENFILE) 
            {
                //do nothing，这个官方做法是先把读事件从listen socket上移除，然后再弄个定时器，定时器到了则继续执行该函数，但是定时器到了有个标记，会把读事件增加到listen socket上去；
                //我这里目前先不处理吧【因为上边已经写这个日志了】；
            }            
            return;
        }

        //走到这里的，表示accept4()成功了        
        newc = ngx_get_connection(s);
        if(newc == NULL)
        {
            //连接池中连接不够用，那么就得把这个socekt直接关闭并返回了
            if(close(s) == -1)
            {
                ngx_log_error_core(NGX_LOG_ALERT,errno,"CSocekt::ngx_event_accept()中close(%d)失败!",s);                
            }
            return;
        }
        //...........将来这里会判断是否连接超过最大允许连接数，现在，这里可以不处理

        //成功的拿到了连接池中的一个连接
        memcpy(&newc->s_sockaddr,&mysockaddr,socklen);  //拷贝客户端地址到连接对象【要转成字符串ip地址参考函数ngx_sock_ntop()】
        //{
        //    //测试将收到的地址弄成字符串，格式形如"192.168.1.126:40904"或者"192.168.1.126"
        //    u_char ipaddr[100]; memset(ipaddr,0,sizeof(ipaddr));
        //    ngx_sock_ntop(&newc->s_sockaddr,1,ipaddr,sizeof(ipaddr)-10); //宽度给小点
        //    ngx_log_stderr(0,"ip信息为%s\n",ipaddr);
        //}

        if(!use_accept4)
        {
            //如果不是用accept4()取得的socket，那么就要设置为非阻塞【因为用accept4()的已经被accept4()设置为非阻塞了】
            if(setnonblocking(s) == false)
            {
                //设置非阻塞居然失败
                ngx_close_accepted_connection(newc);
                return; //直接返回
            }
        }

        newc->listening = oldc->listening;
        newc->w_ready = 1;
        newc->rhandler = &CSocket::ngx_wait_request_handler;
        if(ngx_epoll_add_event(s,                 //socket句柄
                                1,0,              //读，写
                                EPOLLET,          //其他补充标记【EPOLLET(高速模式，边缘触发ET)】
                                EPOLL_CTL_ADD,    //事件类型【增加，还有删除/修改】                                    
                                newc              //连接池中的连接
                                ) == -1)
        {
            //增加事件失败，失败日志在ngx_epoll_add_event中写过了，因此这里不多写啥；
            ngx_close_accepted_connection(newc);
            return; //直接返回
        } 

        break;  //一般就是循环一次就跳出去
    }while (1);   
    return;
}

//用户连入，accept4()时，得到的socket在处理中产生失败，则资源用这个函数释放
void CSocket::ngx_close_accepted_connection(lpngx_connection_t c)
{
    int fd = c->fd;
    ngx_free_connection(c);
    c->fd = -1; 
    if(close(fd) == -1)
    {
        ngx_log_error_core(NGX_LOG_ALERT,errno,"CSocekt::ngx_close_accepted_connection()中close(%d)失败!",fd);  
    }
    return;
}