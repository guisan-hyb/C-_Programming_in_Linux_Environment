#include <stdio.h>
#include <sys/ipc.h>

int main(){
    key_t key = ftok("/tmp", 'a');
    if(key == EOF){
        perror("ftok: ");
        return -1;
    }

    printf("key is: %d\n", key);
    printf("key is: %x\n", key);

    return 0;
}

