//和开启子进程相关

#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<signal.h>
#include<errno.h>
#include<unistd.h>

#include "ngx_func.h"
#include "ngx_macro.h"
#include "ngx_c_conf.h"

//函数声明
static void ngx_start_worker_processes(int threadnums);
static int ngx_spawn_process(int threadnums,const char* pprocname);
static void ngx_worker_process_cycle(int inum,const char* pprocname);
static void ngx_worker_process_init(int inum);

//变量声明
static u_char master_process[] = "master process";

//创建子进程
void ngx_master_process_cycle()
{
    sigset_t set;   //信号集
    sigemptyset(&set);  //清空信号集

    sigaddset(&set,SIGCHLD);
    sigaddset(&set,SIGALRM);
    sigaddset(&set,SIGIO);
    sigaddset(&set,SIGHUP);
    sigaddset(&set,SIGINT);
    sigaddset(&set,SIGUSR1);
    sigaddset(&set,SIGUSR2);
    sigaddset(&set,SIGWINCH);
    sigaddset(&set,SIGTERM);
    sigaddset(&set,SIGQUIT);

    //设置，此时无法接受的信号；阻塞期间，你发过来的上述信号，多个会被合并为一个，暂存着，等你放开信号屏蔽后才能收到这些信号。。。
    if(sigprocmask(SIG_BLOCK,&set,NULL) == -1)
    {
        ngx_log_error_core(NGX_LOG_ALERT,errno,"ngx_master_process_cycle()中sigprocmask()失败！");
    }

    //即使sigprocmask()失败，程序流程也继续往下走

    //首先我设置主进程标题--------begin
    size_t size;
    int i;
    size = sizeof(master_process);
    size += g_argvneedmem;
    if(size<1000)
    {
        char title[1000]={0};
        strcpy(title,(const char*)master_process);
        strcat(title," ");
        for(i=0;i<g_os_argc;i++)
        {
            strcat(title,g_os_argv[i]);
        }
        ngx_setproctitle(title);    //设置标题
        ngx_log_error_core(NGX_LOG_NOTICE,0,"%s %P 启动并开始运行......!",title,ngx_pid); //设置标题时顺便记录下来进程名，进程id等信息到日志
    }
    //首先我设置主进程标题--------end

    //从配置文件中读取要创建的worker进程数量
    CConfig* p_config = CConfig::GetInstance();
    int workprocess = p_config->GetIntDefault("WorkerProcesses",1); 
    ngx_start_worker_processes(workprocess);  //这里要创建worker子进程

    //创建子进程后，父进程的执行流程会返回到这里，子进程不会走进来

    sigemptyset(&set);

    for(;;)
    {
        // ngx_log_error_core(0,0,"haha--这是父进程，pid为%P",ngx_pid);
        sigsuspend(&set);
        sleep(1);
    }

    return;
}

//描述：根据给定的参数创建指定数量的子进程，因为以后可能要扩展功能，增加参数，所以单独写成一个函数
//threadnums:要创建的子进程数量
static void ngx_start_worker_processes(int threadnums)
{
    int i;
    for(i=0;i<threadnums;i++){
        ngx_spawn_process(i,"worker process");
    }
    return;
}

//产生一个子进程
static int ngx_spawn_process(int inum,const char* pprocname)
{
    pid_t pid;
    pid = fork();   //fork()系统调用产生子进程
    switch(pid)     //pid判断父子进程，分支处理
    {
    case -1:  //产生子进程失败
        ngx_log_error_core(NGX_LOG_ALERT,errno,"ngx_spawn_process()fork()产生子进程num=%d,procname=\"%s\"失败！",inum,pprocname);
        return -1;

    case 0:   //子进程分支
        ngx_parent = ngx_pid;
        ngx_pid = getpid();
        ngx_worker_process_cycle(inum,pprocname);
        break;
    default:
        break;
    }
    return pid;
}

//描述：worker子进程的功能函数，每个woker子进程，就在这里循环着了（无限循环【处理网络事件和定时器事件以对外提供web服务】）
static void ngx_worker_process_cycle(int inum,const char* pprocname)
{   
    //设置一下变量
    ngx_process = NGX_PROCESS_WORKER;  //设置进程的类型，是worker进程

    //重新为子进程设置进程名，不要与父进程重复
    ngx_worker_process_init(inum);
    ngx_setproctitle(pprocname);
    ngx_log_error_core(NGX_LOG_NOTICE,0,"%s %P 启动并开始运行......!",pprocname,ngx_pid); //设置标题时顺便记录下来进程名，进程id等信息到日志

    //setvbuf(stdout,NULL,_IONBF,0);
    for(;;){
        
        //ngx_log_error_core(0,0,"good--这是子进程，编号为%d,pid为%P！",inum,ngx_pid);
        ngx_process_events_and_timers();//处理网络事件和定时器事件
    }
    return;
}

//子进程创建时调用本函数进行一些初始化工作
static void ngx_worker_process_init(int inum){
    sigset_t set;   //信号集
    sigemptyset(&set);
    if(sigprocmask(SIG_SETMASK,&set,NULL)==-1){
        ngx_log_error_core(NGX_LOG_ALERT,errno,"ngx_worker_process_init()中sigprocmask()失败!");
    }

    g_socket.ngx_epoll_init();  //初始化epoll相关内容，同时往监听socket上增加监听事件，从而开始让监听端口履行其职责

    return;
}