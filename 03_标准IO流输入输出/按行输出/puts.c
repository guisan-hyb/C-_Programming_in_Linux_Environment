// puts函数用以将指定字符串输出到stdout：
// 原型：int puts(const char *s) 传入参数为被输出的字符串

#include <stdio.h>

int main(){
    puts("Hello");//自动添加\n
    char *s = "World";
    puts(s);

    return 0;
}

