//char * strerror(int errno); 根据错误号，返回错误信息字符串
#include <stdio.h>
#include <string.h>
#include <errno.h>

int main(){
    FILE* fp = fopen("test.txt","r");
    if(fp==NULL){
        printf("错误号是: %d\n",errno);
        printf("错误信息是: %s\n",strerror(errno));
        return -1;
    }
    fclose(fp);

    return 0;
}