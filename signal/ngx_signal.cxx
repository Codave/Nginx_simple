//和信号有关的函数放在这里
#include<string.h>
#include<stdint.h>
#include<stdio.h>
#include<stdlib.h>
#include<signal.h>
#include<errno.h>

#include "ngx_macro.h"
#include "ngx_func.h"

//一个信号有关的结构 ngx_signal_t
typedef struct{
    int            signo;   //信号对应的数字编号
    const char*    signame; //信号对应的中文名字

    //信号处理函数，这个函数有我们自己来提供，但是它的参数和返回值是固定的
    void (*handler)(int signo,siginfo_t* siginfo,void* ucontext);
}ngx_signal_t;

//声明一个信号处理函数
static void ngx_signal_handler(int signo,siginfo_t* siginfo,void* ucontext);

//数组
ngx_signal_t signals[]={
    //signo     signame             handler
    {SIGHUP,    "SIGHUP",           ngx_signal_handler},    //终端断开信号
    {SIGINT,    "SIGINT",           ngx_signal_handler},    //标识2
    {SIGTERM,   "SIGTERM",          ngx_signal_handler},    //标识1s
    {SIGCHLD,  "SIGCHILD",         ngx_signal_handler},
    {SIGQUIT,   "SIGQUIT",          ngx_signal_handler},
    {SIGIO,     "SIGIO",            ngx_signal_handler},
    {SIGSYS,    "SIGSYS, SIG_IGN",  ngx_signal_handler},

    {0,         NULL,           NULL              }
};

//初始化信号的函数，用于注册信号处理程序
//返回值：0成功   -1失败
int ngx_init_signals()
{
    ngx_signal_t*    sig;   //指向自定义结构体数组的指针
    struct sigaction sa;    //sigaction：系统定义的跟信号有关的一个结构

    for(sig = signals;sig->signo!=0;sig++)
    {

        memset(&sa,0,sizeof(struct sigaction));

        if(sig->handler) //如果信号处理函数不为空，handler当然表示我自己的信号处理函数
        {
            sa.sa_sigaction = sig->handler;
            sa.sa_flags = SA_SIGINFO;
        }
        else
        {
            sa.sa_handler = SIG_IGN;
        }

        sigemptyset(&sa.sa_mask);

        //设置信号处理动作(信号处理函数)，说白了这里就是让这个信号来了后调用我的处理程序
        if(sigaction(sig->signo,&sa,NULL)==-1)
        {
            ngx_log_error_core(NGX_LOG_EMERG,errno,"sigaction(%s) failed",sig->signame);
            return -1;
        }
        else
        {
            //ngx_log_error_core(NGX_LOG_EMERG,errno,"sigaction(%s) succed!",sig->signame);     //成功不用写日志 
            ngx_log_stderr(0,"sigaction(%s) succed!",sig->signame); //直接往屏幕上打印看看 ，不需要时可以去掉
        }
    }
    return 0;
}

//信号处理函数
static void ngx_signal_handler(int signo,siginfo_t* siginfo,void* ucontext)
{
    printf("信号来了!\n");
}
















































// //信号处理函数
// static void ngx_signal_handler(int signo,siginfo_t* siginfo,void* ucontext){
//     printf("来信号了\n");
// }

