//和日志相关的函数放在这里

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<stdint.h>  //uintptr_t
#include<stdarg.h>  //va_start
#include<unistd.h>
#include<sys/time.h>   //gettimeofday
#include<time.h>
#include<fcntl.h>   //open
#include<errno.h>

#include "ngx_global.h"
#include "ngx_macro.h"
#include "ngx_func.h"
#include "ngx_c_conf.h"

//全局量
//错误等级，和ngx_macro.h里定义的日志等级宏是一一对应的
static u_char err_levels[][20] = {
    {"stderr"}, //0:控制台错误
    {"emerg"},  //1：紧急
    {"alert"},  //2:警戒
    {"crit"},   //3:严重
    {"error"},  //4:错误
    {"warn"},   //5:警告
    {"notice"}, //6:注意
    {"info"},   //7:信息
    {"debug"}   //8：调试
};
ngx_log_t   ngx_log;

//----------------------------------------------------------------------------------------------------------------------
//描述：通过可变参数组合出字符串【支持...省略号形参】，自动往字符串最末尾增加换行符【所以调用者不用加\n】， 往标准错误上输出这个字符串；
//     如果err不为0，表示有错误，会将该错误编号以及对应的错误信息一并放到组合出的字符串中一起显示；

//调用格式比如：ngx_log_stderr(0, "invalid option: \"%s\",%d", "testinfo",123);
/* 
    ngx_log_stderr(0, "invalid option: \"%s\"", argv[0]);  //nginx: invalid option: "./nginx"
    ngx_log_stderr(0, "invalid option: %10d", 21);         //nginx: invalid option:         21  ---21前面有8个空格
    ngx_log_stderr(0, "invalid option: %.6f", 21.378);     //nginx: invalid option: 21.378000   ---%.这种只跟f配合有效，往末尾填充0
    ngx_log_stderr(0, "invalid option: %.6f", 12.999);     //nginx: invalid option: 12.999000
    ngx_log_stderr(0, "invalid option: %.2f", 12.999);     //nginx: invalid option: 13.00
    ngx_log_stderr(0, "invalid option: %xd", 1678);        //nginx: invalid option: 68E
    ngx_log_stderr(0, "invalid option: %Xd", 1678);        //nginx: invalid option: 68E
    ngx_log_stderr(15, "invalid option: %s , %d", "testInfo",326);        //nginx: invalid option: testInfo , 326
    ngx_log_stderr(0, "invalid option: %d", 1678); 
*/

void ngx_log_stderr(int err,const char* fmt,...)
{
    va_list args;   //创建一个va_list类型变量
    u_char errstr[NGX_MAX_ERROR_STR+1];  //2048
    u_char *p,*last;

    memset(errstr,0,sizeof(errstr));

    last=errstr+NGX_MAX_ERROR_STR;

    p=ngx_cpymem(errstr,"nginx: ",7);

    va_start(args,fmt);     //使args指向起始的参数
    p=ngx_vslprintf(p,last,fmt,args);   //组合出这个字符串保存在errstr里
    va_end(args);   //释放args

    if(err) //如果错误代码不是0，表示有错误发生
    {   
        //错误代码和错误信息也要显示出来
        p=ngx_log_errno(p,last,err);
    }

    //若位置不够，那换行也要硬插入到末尾，哪怕覆盖到其他内容
    if(p>=(last-1)){
        p=(last-1)-1;
    }

    *p++ = '\n';

    //往标准错误【一般是屏幕】输出信息
    write(STDERR_FILENO,errstr,p-errstr);

    return;
}

u_char* ngx_log_errno(u_char* buf,u_char* last,int err)
{
    char* perrorinfo = strerror(err);
    size_t len = strlen(perrorinfo);

    char leftstr[10]={0};
    sprintf(leftstr," (%d: ",err);
    size_t leftlen=strlen(leftstr);

    char rightstr[] = ") ";
    size_t rightlen = strlen(rightstr);

    size_t extralen = leftlen + rightlen;   //左右的额外宽度
    if((buf+len+extralen)<last){
        //保证整个我装得下，我就装，否则我就全部抛弃，nginx的做法是 如果存不下，就硬留50个字符
        buf = ngx_cpymem(buf,leftstr,leftlen);
        buf = ngx_cpymem(buf,perrorinfo,len);
        buf = ngx_cpymem(buf,rightstr,rightlen);
    }
    return buf;
}


void ngx_log_error_core(int level,int err,const char* fmt,...)
{
    u_char* last;
    u_char errstr[NGX_MAX_ERROR_STR+1];

    memset(errstr,0,sizeof(errstr));
    last = errstr+NGX_MAX_ERROR_STR;

    struct timeval tv;
    struct tm      tm;
    time_t         sec;
    u_char         *p;
    va_list        args;

    memset(&tv,0,sizeof(struct timeval));
    memset(&tm,0,sizeof(struct tm));

    gettimeofday(&tv,NULL);

    sec = tv.tv_sec;             //秒
    localtime_r(&sec, &tm);      //把参数1的time_t转换为本地时间，保存到参数2中去，带_r的是线程安全的版本，尽量使用
    tm.tm_mon++;                 //月份要调整下正常
    tm.tm_year += 1900;          //年份要调整下才正常
    
    u_char strcurrtime[40]={0};  //先组合出一个当前时间字符串，格式形如：2019/01/08 19:57:11
    ngx_slprintf(strcurrtime,  
                    (u_char *)-1,                       //若用一个u_char *接一个 (u_char *)-1,则 得到的结果是 0xffffffff....，这个值足够大
                    "%4d/%02d/%02d %02d:%02d:%02d",     //格式是 年/月/日 时:分:秒
                    tm.tm_year, tm.tm_mon,
                    tm.tm_mday, tm.tm_hour,
                    tm.tm_min, tm.tm_sec);
    p = ngx_cpymem(errstr,strcurrtime,strlen((const char *)strcurrtime));  //日期增加进来，得到形如：     2019/01/08 20:26:07
    p = ngx_slprintf(p, last, " [%s] ", err_levels[level]);                //日志级别增加进来，得到形如：  2019/01/08 20:26:07 [crit] 
    p = ngx_slprintf(p, last, "%P: ",ngx_pid);                             //支持%P格式，进程id增加进来，得到形如：   2019/01/08 20:50:15 [crit] 2037:

    va_start(args, fmt);                     //使args指向起始的参数
    p = ngx_vslprintf(p, last, fmt, args);   //把fmt和args参数弄进去，组合出来这个字符串
    va_end(args);                            //释放args 

    if (err)  //如果错误代码不是0，表示有错误发生
    {
        //错误代码和错误信息也要显示出来
        p = ngx_log_errno(p, last, err);
    }
    //若位置不够，那换行也要硬插入到末尾，哪怕覆盖到其他内容
    if (p >= (last - 1))
    {
        p = (last - 1) - 1; //把尾部空格留出来，这里感觉nginx处理的似乎就不对 
                             //我觉得，last-1，才是最后 一个而有效的内存，而这个位置要保存\0，所以我认为再减1，这个位置，才适合保存\n
    }
    *p++ = '\n'; //增加个换行符 

    //这么写代码是图方便：随时可以把流程弄到while后边去；大家可以借鉴一下这种写法
    ssize_t   n;
    while(1) 
    {        
        if (level > ngx_log.log_level) 
        {
            //要打印的这个日志的等级太落后（等级数字太大，比配置文件中的数字大)
            //这种日志就不打印了
            break;
        }
        //磁盘是否满了的判断，先算了吧，还是由管理员保证这个事情吧； 

        //写日志文件        
        n = write(ngx_log.fd,errstr,p - errstr);  //文件写入成功后，如果中途
        if (n == -1) 
        {
            //写失败有问题
            if(errno == ENOSPC) //写失败，且原因是磁盘没空间了
            {
                //磁盘没空间了
                //没空间还写个毛线啊
                //先do nothing吧；
            }
            else
            {
                //这是有其他错误，那么我考虑把这个错误显示到标准错误设备吧；
                if(ngx_log.fd != STDERR_FILENO) //当前是定位到文件的，则条件成立
                {
                    n = write(STDERR_FILENO,errstr,p - errstr);
                }
            }
        }
        break;
    } //end while    
    return;
}

//描述：日志初始化，就是把日志文件打开
void ngx_log_init()
{
    u_char* plogname = NULL;
    size_t nlen;

    //从配置文件中读取和日志相关的配置文件
    CConfig* p_config = CConfig::GetInstance();
    plogname = (u_char*)p_config->GetString("Log");
    if(plogname==NULL)
    {
        //没读到，就要给个缺省的路径文件名了
        plogname = (u_char*)NGX_ERROR_LOG_PATH;
    }
    ngx_log.log_level = p_config->GetIntDefault("LogLevel",NGX_LOG_NOTICE); //缺省日志等级为6【注意】，如果失败，就给缺省日志等级

    ngx_log.fd = open((const char*)plogname,O_WRONLY|O_APPEND|O_CREAT,0644);
    if(ngx_log.fd==-1){
        ngx_log_stderr(errno,"[alter] could not open error log file: open() \"%s\" failed", plogname);
        ngx_log.fd = STDERR_FILENO; //直接定位到标准错误去了
    }
    return;
}