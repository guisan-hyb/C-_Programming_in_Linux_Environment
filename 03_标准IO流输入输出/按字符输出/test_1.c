#include <stdio.h>
#include <string.h>

int main(){
    char* msg = "this is a test\nyou know";
    FILE* fp = fopen("test.txt","a");
    if(fp==NULL){
        perror("打开文件失败!");
        return -1;
    }

    for(int i = 0; i < strlen(msg); i++){
        fputc(msg[i], fp);
    }
    fputc('\n', fp);
    

    return 0;
}