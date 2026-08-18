#include <stdio.h>

int main(){
    FILE* fp = fopen("test.txt","w");
    fclose(fp);

    return 0;
}