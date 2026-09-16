#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>

// 管道文件的内容都在内存中，所以文件大小一直为0
int main(){
    if(mkfifo("myfifo",0666) == -1){
        perror("mkfifo: ");
        return -1;
    }
    printf("FIFO created successfully\n");

    return 0;
}
