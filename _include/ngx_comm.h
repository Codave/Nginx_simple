#ifndef __NGX_COMM_H__
#define __NGX_COMM_H__

//宏定义
#define _PKG_MAX_LENGTH 3000    //每个包的最大长度

//通信 收包状态定义
#define _PKG_HD_INIT        0   //初始状态，准备接受数据包头
#define _PKG_HD_RECVING     1   //接收包头中，包头不完整，继续接收中
#define _PKG_BD_INIT        2   //包头刚好收完，准备接受包体
#define _PKG_BD_RECVING     3   //接收包体中，包体不完整，继续接收中，处理后直接回到_PKG_HD_INIT状态

#define _DATA_BUFSIZE_      20  //要先收包头，我希望定义一个固定大小的数组专门用来收包头

//结构定义
#pragma pack(1) //字节对齐方式，1字节对齐

//包头结构
typedef struct _COMM_PKG_HEADER
{
    unsigned short pkgLen;      //报文总长度 2字节
    unsigned short msgCode;     //消息类型代码 2字节
    int     crc32;              //CRC32效验 4字节 
}COMM_PKG_HEADER,*LPCOMM_PKG_HEADER;

#pragma pack()  //取消指定对齐，恢复缺省对齐


#endif