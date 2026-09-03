#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(){
    printf("using exit...\n");
    printf("This is the end");
    _exit(0);

    return 0;
}

