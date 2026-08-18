// getc函数，支持从流（各类文件）中读取一个字符。
// 原型：int getc(FILE* stream) 读取成功返回字符，失败返回EOF(-1)

// getc函数和fgetc函数的用法一致，但不同的点在于：
// • fgetc是库（stdio.h）函数
// • getc是一个宏函数
// getc不可以传递带有副作用的表达式作为参数

#include <stdio.h>

int main(){
    FILE* fp = fopen("/home/guisan/MyCode/C++_Programming_in_Linux_Environment/03_标准IO流输入输出/按字符输入/ddd.txt","r");
    if(fp==NULL){
        perror("读取失败: ");
        return -1;
    }

    int ch = 0;
    while((ch=getc(fp))!=EOF){
        printf("%c",ch);
    }
    fclose(fp);

    return 0;
}

