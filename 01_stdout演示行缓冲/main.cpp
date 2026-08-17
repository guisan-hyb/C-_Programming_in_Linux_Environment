#include <stdio.h>
#include <unistd.h>

int main(){
    fprintf(stdout,"1\n");
    sleep(3);
    fprintf(stdout,"2 ");
    sleep(3);
    fprintf(stdout,"3\n");

    return 0;
}