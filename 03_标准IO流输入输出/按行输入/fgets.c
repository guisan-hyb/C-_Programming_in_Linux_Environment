// fgets函数用于从指定文件流读取一行内容（换行符为界定），将读入的内容记录到指定的字符串中： 原型：char *fgets(char *s, int buf_size, FILE *stream);
// 参数1：记录读入数据的字符数组
// 参数2：缓冲区大小(byte)， 1个字符占用1个byte
// 参数3：文件流
//     同理，存入数据的字符数据最后1位用以记录\0,
//     有效记录字符数为buf_size - 1


#include <stdio.h>

int main(){
    char s[5];
    char *p = fgets(s, 5, stdin);
    printf("%s\n", s);

    return 0;
}

