// 可以通过setvbuf函数，设置流的缓冲区模式： int setvbuf(FILE *stream, char *buffer, int mode, size_t size)
// 参数：
// ● stream；文件流
// ● buffer；用户提供的缓冲区（设置为NULL系统自动分配内存空间做缓冲区）
// ● mode；缓冲区模式
// ● size；缓冲区大小（bytes），2 <= size <= INT_MAX(2147483647)
// 缓冲区模式有：
// ● _IONBF : 无缓冲
// ● _IOLBF : 行缓冲（在Win32系统即Windows系统和全缓冲一致）
// ● _IOFBF : 全缓冲

#include <stdio.h>

int main(){
    char buf[1024] = {0};
    //设置流为无缓冲模式
    int r = setvbuf(stdout, buf, _IONBF, 1024);
    printf("setvbuf返回值: %d\n", r);
    fputs("haha", stdout);
    printf("test......");
    fgetc(stdin);

    return 0;
}


