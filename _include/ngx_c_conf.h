
#ifndef __NGX_CONF_H__
#define __NGX_CONF_H__

#include<vector>

#include "ngx_global.h" //一些全局/通用定义

//类名可以遵照一定得命名规范，第一个C表示Class
class CConfig
{
private:
    CConfig();
public:
    ~CConfig();
private:
    static CConfig* m_instance;

public:
    static CConfig* GetInstance()
    {
        if(m_instance==NULL)
        {
            //锁
            if(m_instance==NULL)
            {
                m_instance=new CConfig();
                static CGarhuishou c1;
            }
            //放锁
        }
        return m_instance;
    }
    
    class CGarhuishou
    {
    public:
        ~CGarhuishou()
        {
            if(CConfig::m_instance)
            {
                delete CConfig::m_instance;
                CConfig::m_instance=NULL;
            }
        }
    };
public:
    bool Load(const char* pconfName);   //装载配置文件
    const char* GetString(const char* p_itemname);
    int GetIntDefault(const char* p_itemname,const int def);

public:
    std::vector<LPCConfItem> m_ConfigItemList; //存储配置信息的列表
    
};

#endif