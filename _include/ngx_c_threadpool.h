#ifndef __NGX_THREADPOOL_H__
#define __NGX_THREADPOOL_H__

#include<vector>
#include<pthread.h>
#include<atomic>

//线程池相关类
class CThreadPool
{
public:
    //构造函数
    CThreadPool();
    //析构函数
    ~CThreadPool();
public:
    bool Create(int threadNum);
    void StopAll();
    void Call(int irmqc);
private:
    static void* ThreadFunc(void* threadData);
private:
    struct ThreadItem
    {
        pthread_t   _Handle;    //线程句柄
        CThreadPool* _pThis;    //记录线程池的指针
        bool      ifrunning;    //编辑是否正式启动起来，启动起来后，才允许调用StopAll()来释放

        //构造函数
        ThreadItem(CThreadPool* pthis):_pThis(pthis),ifrunning(false){}
        //析构函数
        ~ThreadItem(){}
    };
private:
    static pthread_mutex_t    m_pthreadMutex;   //线程同步互斥量/线程同步锁
    static pthread_cond_t     m_pthreadCond;   //线程同步条件变量
    static bool               m_shutdown;       //线程退出标志,false不退出，true退出

    int                       m_iThreadNum;     //要创建的线程数量

    std::atomic<int>        m_iRunningThreadNum;    //线程数，运行中的线程数
    time_t                  m_iLastEmgTime;         //上次发生线程不够用【紧急事件】的时间，防止日志报的太频繁

    std::vector<ThreadItem*> m_threadVector;         //线程容器
};

#endif