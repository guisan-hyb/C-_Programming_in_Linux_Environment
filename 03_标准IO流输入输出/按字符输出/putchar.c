//puchar函数可以将一个字符输出到stdout（标准输出、默认就是控制台）：
//原型：int putchar(int c) 传入参数为被输出的字符

#include <stdio.h>

int main(){
    char ch = 'c';
    putchar(ch);
    putchar('\n');

    return 0;
}

