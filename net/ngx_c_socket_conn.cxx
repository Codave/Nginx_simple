//和网络 中 连接/连接池 有关的函数放这里

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
#include "ngx_c_memory.h"

//从连接池中获得一个空闲连接
lpngx_connection_t CSocket::ngx_get_connection(int isock)
{
    lpngx_connection_t c = m_pfree_connections;     //空闲连接链表头

    if(c==NULL){
        //空闲连接被耗尽
        ngx_log_stderr(0,"CSocket::ngx_get_connection()中空闲链表为空！");
        return NULL;
    }

    m_pfree_connections = c->data;  //指向连接池中下一个未用的节点
    m_free_connection_n--;          //空闲连接少1

    //(1)先把c指向的对象中有用的东西搞出来保存成变量,下面要填回去
    uintptr_t instance = c->instance;
    uint64_t  iCurrsequence = c->iCurrsequence;

    //(2)复制出来后，清空并给适当值
    memset(c,0,sizeof(ngx_connection_t));
    c->fd = isock;      //套接字要保存起来
    c->curStat = _PKG_HD_INIT;  //收包状态处于  初始状态

    c->precvbuf = c->dataHeadInfo;
    c->irecvlen = sizeof(COMM_PKG_HEADER);

    c->ifnewrecvMem = false;
    c->pnewMemPointer = NULL;

    //(3)
    c->instance = !instance;
    c->iCurrsequence = iCurrsequence;
    ++c->iCurrsequence;     //每次取用该值都增加1

    return c;
}

//归还参数c所代表的连接到到连接池中，注意参数类型是lpngx_connection_t
void CSocket::ngx_free_connection(lpngx_connection_t c) 
{
    c->data = m_pfree_connections;                       //回收的节点指向原来串起来的空闲链的链头

    //节点本身也要干一些事
    ++c->iCurrsequence;                                  //回收后，该值就增加1,以用于判断某些网络事件是否过期【一被释放就立即+1也是有必要的】

    m_pfree_connections = c;                             //修改 原来的链头使链头指向新节点
    ++m_free_connection_n;                               //空闲连接多1    
    return;
}

//用户连入，accept4()时，得到的socket在处理中产生失败，则资源用这个函数释放
//我们把ngx_close_accepted_connection()函数改名为让名字更通用，并从文件ngx_socket_accept.cxx迁移到本文件中，并改造其中代码，注意顺序
void CSocket::ngx_close_connection(lpngx_connection_t c)
{
    if(close(c->fd) == -1)
    {
        ngx_log_error_core(NGX_LOG_ALERT,errno,"CSocekt::ngx_close_accepted_connection()中close(%d)失败!",c->fd);  
    }
    c->fd = -1; 
    ngx_free_connection(c);
    return;
}