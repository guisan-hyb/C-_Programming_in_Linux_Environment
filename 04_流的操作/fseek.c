// fseek函数可以重置定位流的offset
// 原型：long fseek(FILE *stream, long offset, int whence);
// ● whence表示起始位置，可用：SEEK_SET(开头) / SEEK_CUR(当前) / SEEK_END(结尾)
// ● offset表示基于whence所示位置的偏移量，正数向后负数向前

#include <stdio.h>

int main(){
    FILE *fp = fopen("test.txt", "r");
    if(fp==NULL){
        perror("打开文件失败: ");
        return -1;
    }

    char line[1024] = {0};
    fgets(line, 1024, fp);
    printf("%s", line);
    fgets(line, 1024, fp);
    printf("%s", line);

    //重置流的位置 -> 重置为开头
    fseek(fp, 0, SEEK_SET);

    //再次读取文件
    fgets(line, 1024, fp);
    printf("%s", line);

    fseek(fp, 0, SEEK_SET);

    fgets(line, 1024, fp);
    printf("%s", line);

    fclose(fp);

    return 0;
}

