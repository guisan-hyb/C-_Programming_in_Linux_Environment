// pftell返回指定流当前的offset值;失败返回EOF
// long  ftell(FILE *stream);

#include <stdio.h>
#include <stdlib.h>

int main(){
    FILE *fp = fopen("test.txt", "w");
    if(fp==NULL){
        perror("打开文件失败: ");
        return -1;
    }

    printf("写出之前offset: %ld\n", ftell(fp));
    for (int i = 65; i < 65 + 26;i++){
        fputc(i, fp);
        printf("写入: %c之后, offset: %ld\n", i, ftell(fp));
    }

    fclose(fp);

    return 0;
}
