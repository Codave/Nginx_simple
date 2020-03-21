//和信号有关的函数放在这里
#include<string.h>
#include<stdint.h>
#include<stdio.h>
#include<stdlib.h>
#include<signal.h>
#include<errno.h>
#include<sys/wait.h>

#include "ngx_global.h"
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
static void ngx_process_get_status(void);//获取子进程的结束状态，防止单独kill子进程时子进程变成僵尸进程

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
            //ngx_log_stderr(0,"sigaction(%s) succed!",sig->signame); //直接往屏幕上打印看看 ，不需要时可以去掉
        }
    }
    return 0;
}

//信号处理函数
static void ngx_signal_handler(int signo,siginfo_t* siginfo,void* ucontext)
{
    //printf("信号来了!\n");
    ngx_signal_t* sig;
    char*         action;//一个字符串，用于记录一个动作字符串以往日志文件中写

    for(sig=signals;sig->signo!=0;sig++)
    {
        //找到对应信号，即可处理
        if(sig->signo==signo)
        {
            break;
        }
    }

    action = (char*)"";

    if(ngx_process == NGX_PROCESS_MASTER)
    {
        //master进程的往这里走
        switch(signo)
        {
        case SIGCHLD:
            ngx_reap = 1;//标记子进程状态变化，日后master主进程的for(;;)循环中可能会用到这个变量【比如重新产生一个子进程】
            break;
        
        default:
            break;
        }
    }
    else if(ngx_process == NGX_PROCESS_WORKER)
    {
        //worker进程的往这里走
        //......以后再增加
    }
    else
    {
        //非master非worker进程，先啥也不干
    }
    
    //这里记录一些日志信息
    if(siginfo && siginfo->si_pid)  //si_pid = sending process ID【发送该信号的进程id】
    {
        ngx_log_error_core(NGX_LOG_NOTICE,0,"signal %d (%s) received from %P%s", signo, sig->signame, siginfo->si_pid, action); 
    }
    else
    {
        ngx_log_error_core(NGX_LOG_NOTICE,0,"signal %d (%s) received %s",signo, sig->signame, action);//没有发送该信号的进程id，所以不显示发送该信号的进程id
    }

     //子进程状态有变化，通常是意外退出
    if (signo == SIGCHLD) 
    {
        ngx_process_get_status(); //获取子进程的结束状态
    } 

    return;
}

//获取子进程的结束状态，防止单独kill子进程时子进程变成僵尸进程
static void ngx_process_get_status(void)
{
    pid_t            pid;
    int              status;
    int              err;
    int              one=0; //抄自官方nginx，应该是标记信号正常处理过一次

    //当你杀死一个子进程时，父进程会收到这个SIGCHLD信号。
    for ( ;; ) 
    {
        pid = waitpid(-1, &status, WNOHANG); 

        if(pid == 0) //子进程没结束，会立即返回这个数字
        {
            return;
        } 

        if(pid == -1)//这表示这个waitpid调用有错误
        {
            err = errno;
            if(err == EINTR)           //调用被某个信号中断
            {
                continue;
            }

            if(err == ECHILD  && one)  //没有子进程
            {
                return;
            }

            if (err == ECHILD)         //没有子进程
            {
                ngx_log_error_core(NGX_LOG_INFO,err,"waitpid() failed!");
                return;
            }
            ngx_log_error_core(NGX_LOG_ALERT,err,"waitpid() failed!");
            return;
        } 
        
        //走到这里，表示  成功【返回进程id】 ，这里根据官方写法，打印一些日志来记录子进程的退出
        one = 1;  //标记waitpid()返回了正常的返回值
        if(WTERMSIG(status))  //获取使子进程终止的信号编号
        {
            ngx_log_error_core(NGX_LOG_ALERT,0,"pid = %P exited on signal %d!",pid,WTERMSIG(status)); //获取使子进程终止的信号编号
        }
        else
        {
            ngx_log_error_core(NGX_LOG_NOTICE,0,"pid = %P exited with code %d!",pid,WEXITSTATUS(status)); //WEXITSTATUS()获取子进程传递给exit或者_exit参数的低八位
        }
    } 
    return;
}















































// //信号处理函数
// static void ngx_signal_handler(int signo,siginfo_t* siginfo,void* ucontext){
//     printf("来信号了\n");
// }

