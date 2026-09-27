#pragma once

#define BUFF_SIZE 2048  //缓存区大小
#define HEAD_LEN 4      //头部长度4字节
#define HEAD_ID_LEN 2   //头部id长度2字节
#define HEAD_LEN_LEN 2  //头部长度字段2字节

#define  IO_CONTINUE 2  //继续IO操作
#define  IO_EAGAIN 1    //非阻塞IO EAGAIN
#define  IO_ERROR -1    //对端关闭或者网络错误
#define  IO_SUCCESS 0   //读写成功

