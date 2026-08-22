// ferror判断流是否出错，返回值0表示逻辑假，返回值1表示逻辑真

#include <stdio.h>

int main(){
    FILE *fp = fopen("test.txt", "r");
    if(fp==NULL){
        perror("打开文件失败: ");
        return -1;
    }

    if(ferror(fp)){
        perror("流出错，错误：");
    }else{
        printf("流一切正常\n");
    }

    fclose(fp);

    return 0;
}

