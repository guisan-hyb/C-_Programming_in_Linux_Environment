#include <stdio.h>
#include <stdlib.h>

int main(){
    FILE *fp = fopen("/home/guisan/MyCode/C++_Programming_in_Linux_Environment/03_标准IO流输入输出/test.txt", "r");
    if(fp==NULL){
        perror("文件打开失败: ");
        return -1;
    }

    //初始化读取的数据
    char data[4096] = {0};
    char* ch = NULL;
    while((ch=fgets(data,4096,fp))!=NULL){
        printf("%s", data);
    }
    fclose(fp);

    return 0;
}