

#include<stdio.h>
#include<unistd.h>

void mysignal()
{
    printf("执行了mysignal()函数!\n");
    return;
}












// #include "ngx_macro.h"
// #include "ngx_func.h"

// //一个信号有关的结构 ngx_signal_t
// typedef struct{
//     int            signo;   //信号对应的数字编号
//     const char*    signame; //信号对应的中文名字

//     //信号处理函数，这个函数有我们自己来提供，但是它的参数和返回值是固定的
//     void (*handler)(int signo,siginfo_t* siginfo,void* ucontext);
// }ngx_signal_t;

// //声明一个信号处理函数
// static void ngx_signal_handler(int signo,siginfo_t* siginfo,void* ucontext);



// ngx_signal_t signals[]={
//     //signo     signame             handler
//     {SIGHUP,    "SIGHUP",           ngx_signal_handler},    //终端断开信号
//     {SIGINT,    "SIGINT",           ngx_signal_handler},    //标识2
//     {SIGTERM,   "SIGTERM",          ngx_signal_handler},    //标识1s
//     {SIGCHILD,  "SIGCHILD",         ngx_signal_handler},
//     {SIGQUIT,   "SIGQUIT",          ngx_signal_handler},
//     {SIGIO,     "SIGIO",            ngx_signal_handler},
//     {SIGSYS,    "SIGSYS, SIG_IGN",  ngx_signal_handler},



//     {0,         NULL,           NULL              }

// };


// int ngx_init_signals()
// {
//     ngx_signal_t*    sig;
//     struct sigaction sa;

//     for(sig = signals;sig->signo!=0;sig++)
//     {

//         memset(&sa,0,sizeof(struct sigaction));
//     }
// }






















































// //信号处理函数
// static void ngx_signal_handler(int signo,siginfo_t* siginfo,void* ucontext){
//     printf("来信号了\n");
// }

