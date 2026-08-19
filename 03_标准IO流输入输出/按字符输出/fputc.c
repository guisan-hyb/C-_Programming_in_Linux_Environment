// fputc函数可以将一个字符输出到指定流中
// 原型： int fputc(int c,  FILE* stream);  传入参数为被输出的字符和需要输出的文件流指针

#include <stdio.h>

//其一：
// int main(){
//     char ch = 'a';
//     fputc(ch,stdout);
//     fputc('\n',stdout);

//     return 0;
// }

//其二：
int main(){
    FILE* fp = fopen("test.txt","w");
    fputc('h',fp);
    fputc('e',fp);
    fputc('l',fp);
    fputc('l',fp);
    fputc('o',fp);
    fputc('\n',fp);

    fclose(fp);

    return 0;
}