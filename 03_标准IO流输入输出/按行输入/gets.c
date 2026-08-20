// gets不推荐使用，容易造成缓冲区溢出
// gets函数用于从stdin读取一行内容(换行符为界定)，将读入的内容记录到指定的字符中
// 原型： char *gets(char *s); 传入参数是字符数组

#include <stdio.h>

int main(){
    char s[5];
    char *p = gets(s);
    printf("%s\n", s);

    return 0;
}