// feof判断流是否到达文件末尾，返回值0表示逻辑假，返回值1表示逻辑真

#include <stdio.h>

int main(){
    FILE *fp = fopen("test.txt", "r");
    if(fp==NULL){
        perror("");
        return -1;
    }

    char line[1024] = {0};
    fgets(line, 1024, fp);
    printf("流是否达到末尾: %s\n", feof(fp) ? "Y" : "N");

    fgets(line, 1024, fp);
    printf("流是否达到末尾: %s\n", feof(fp) ? "Y" : "N");

    fgets(line, 1024, fp);
    printf("流是否达到末尾: %s\n", feof(fp) ? "Y" : "N");

    fgets(line, 1024, fp);
    printf("流是否达到末尾: %s\n", feof(fp) ? "Y" : "N");

    return 0;
}

