// 基于fread/fwrite完成文件复制（要求非文本文件，比如mp4视频文件）

#include <stdio.h>
#include <stdlib.h>

#define N 1024

int main(){
    FILE *src_file = fopen("src.mp4", "r");
    if(src_file==NULL){
        perror("无法打开文件: ");
        return -1;
    }

    FILE *dest_file = fopen("dest.mp4", "w");
    if(dest_file==NULL){
        perror("无法创建文件: ");
        fclose(src_file);
        return -1;
    }

    size_t element_num = 0;
    char buffer[N] = {0};
    // 每次读取N字节写入目的文件
    while((element_num = fread(buffer,1,N,src_file)) > 0){
        fwrite(buffer, 1, element_num, dest_file);
    }
    fclose(src_file);
    fclose(dest_file);

    return 0;
}

