#include <stdio.h>

int main(){
    FILE* fp = fopen("test.txt","r");
    char line[1024] = "";
    while(fgets(line,1024,fp)!=NULL){
        printf("%s",line);
    }

    fclose(fp);

    return 0;
}