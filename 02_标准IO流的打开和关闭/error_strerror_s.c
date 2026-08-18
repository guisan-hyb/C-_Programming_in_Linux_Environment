//为了避免strerror的C4996警告，可以使用strerror_s函数
// #define __STDC_WANT_LIB_EXT1__ 1
// #include <locale.h>
// #include <stdio.h>
// #include <errno.h>
// #include <string.h>

// int main(){
//     FILE* fp = fopen("test.txt","r");
//     if(fp==NULL){
//         printf("错误号是: %d\n",errno);
//         char err_msg[1024]="";

//         #ifdef __STDC_LIB_EXT1__ //判断编译器是否支持C11标准扩展
//         strerror_s(err_msg,1024,errno);
//         printf("错误信息是: %s",err_msg);
//         #endif

//         return -1;
//     }
//     fclose(fp);

//     return 0;
// }


//strerror_s 属于 Windows 的安全函数，在 Linux 的 gcc 下不被支持


//使用 POSIX/XSI 版本的 strerror_r（跨 POSIX 平台更标准）
//
// #define _GNU_SOURCE
// #include <stdio.h>
// #include <string.h>
// #include <errno.h>

// int main() {
//     char buf[100];
    
//     // XSI 版本返回 int，0表示成功
//     int ret = strerror_r(errno, buf, sizeof(buf));
//     if (ret == 0) {
//         printf("错误号是: %d\n", errno);
//         printf("错误信息是: %s\n", buf);
//     } else {
//         printf("无法获取错误信息\n");
//     }
    
//     return 0;
// }




//使用 GNU 版本的 strerror_r（Linux 下最常用）
//
#define _GNU_SOURCE //必须添加宏定义
#include <stdio.h>
#include <string.h>
#include <errno.h>

int main() {
    // 假设这里打开文件失败了，errno 被设置为 2
    char buf[100];
    
    // GNU 版本返回 char* 指针
    char *err_msg = strerror_r(errno, buf, sizeof(buf));
    printf("错误号是: %d\n", errno);
    printf("错误信息是: %s\n", err_msg); // 或者打印 buf 也可以（GNU版本buf里也有内容）
    
    return 0;
}

