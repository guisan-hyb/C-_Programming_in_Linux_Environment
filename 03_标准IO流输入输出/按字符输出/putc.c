// putc函数可将一个字符输出到指定流中
// 原型 int putc(int c, FILE* stream);  传入参数为被输出的字符和需要输出的文件流指针

#include <stdio.h>

//其一：
// int main(){
//     char ch = 'a';
//     putc(ch,stdout);
//     putc('\n',stdout);

//     return 0;
// }


//其二：
int main(){
    FILE* fp = fopen("test.txt","a");
    putc('w',fp);
    putc('o',fp);
    putc('r',fp);
    putc('l',fp);
    putc('d',fp);
    putc('\n',fp);

    return 0;
}

