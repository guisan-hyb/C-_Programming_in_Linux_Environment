// fgetc函数，支持从流（各类文件）中读取一个字符。
// 原型：int fgetc(FILE * stream); 读取成功返回字符，失败返回EOF（-1）

#include <stdio.h>

int main()
{
    // 读取标准输入流
    //
    //  char ch;
    //  ch = fgetc(stdin);
    //  //注意要加换行符号，因为该字符暂存在行缓冲里，
    //  //没有遇见\n就return 0了 （stdout是行缓冲）
    //  printf("%c\n",ch);

    // 读取文件流
    FILE *fp = fopen("/home/guisan/MyCode/C++_Programming_in_Linux_Environment/03_标准IO流输入输出/按字符输入/ddd.txt", "r");
    if (fp == NULL)
    {
        perror("读取文件出错: ");
        return -1;
    }

    int ch = 0;
    // 循环读取直到末尾
    while ((ch = fgetc(fp)) != EOF)
    {
        printf("%c", ch);
    }
    fclose(fp);

    return 0;
}


//健壮性写法：
// #include <stdio.h>

// int main() {
//     FILE *fp = fopen("ddd.txt", "r");
//     if (fp == NULL) {
//         perror("打开文件失败");
//         return 1;
//     }

//     int ch;
//     int last_ch = EOF; // 记录上一个读取的字符

//     // 循环读取
//     while ((ch = fgetc(fp)) != EOF) {
//         printf("%c", ch);
//         last_ch = ch;   // 更新最后读取的字符
//     }

//     // 核心逻辑：如果文件非空，且最后一个字符不是换行符，则补一个换行
//     if (last_ch != EOF && last_ch != '\n') {
//         printf("\n"); 
//     }

//     fclose(fp);
//     return 0;
// }

