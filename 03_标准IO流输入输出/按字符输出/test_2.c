#include <stdio.h>

int main(int argc, char* argv[]){ //C 语言中标准的主函数写法，用于接收命令行参数
    if(argc!=3){
        fprintf(stderr, "用法：%s <源文件> <目标文件> \n", argv[0]);
        return -1;
    }

    const char *sourceFile = argv[1];
    const char *destFile = argv[2];

    //打开源文件
    FILE *src = fopen(sourceFile, "r");
    if(src==NULL){
        perror("无法打开源文件");
        return -1;
    }

    //打开目的文件
    FILE *dest = fopen(destFile, "w");
    if(dest==NULL){
        perror("无法创建目的文件");
        fclose(src);//记得关闭源文件
        return -1;
    }

    int ch = 0;
    while((ch=fgetc(src))!=EOF){
        fputc(ch, dest);
    }
    //fputc('\n', dest);

    fclose(src);
    fclose(dest);

    return 0;
}
