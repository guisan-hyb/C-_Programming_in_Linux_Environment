#include <stdio.h>

int main(){
    unsigned int x = 0x12345678;
    unsigned char *c = (unsigned char *)&x;

    if(*c == 0x12){
        printf("系统采用大端序\n");
    }else if(*c == 0x78){
        printf("系统采用小端序\n");
    }else{
        printf("未知字节序\n");
    }

    return 0;
}
