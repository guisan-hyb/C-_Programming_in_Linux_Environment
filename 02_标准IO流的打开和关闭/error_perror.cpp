//void perror(const char *s); 打印错误信息，输出用户提供字符串s和当前错误
#include <stdio.h>

int main(){
    FILE* fp = fopen("test.txt","r");
    if(fp==NULL){
        perror("错误信息是: ");
        return -1;
    }
    fclose(fp);

    return 0;
}