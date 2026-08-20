// 原型 : int fputs(const char *s, FILE *stream);
// 参数1：被输出字符串 ， 参数2 ： 输出文件流

#include <stdio.h>

int main(){
    // 输出到标准输出，不自动添加\n
    fputs("Hello\n", stdout);

    FILE *fp = fopen("/home/guisan/MyCode/C++_Programming_in_Linux_Environment/03_标准IO流输入输出/test.txt", "a");
    if(fp==NULL){
        perror("打开文件失败: ");
        return -1;
    }

    const char *s = "aaabbbcc\nddd";
    // 返回值非-1表示成功，否则返回EOF(-1)
    int d = fputs(s, fp);
    printf("返回值: %d\n", d);
    if(d==-1){
        perror("写入文件错误: ");
    }
    fclose(fp);

    return 0;
}

