// ● printf可以格式化输出字符串到stdout文件中
// ● fprintf用法和printf一样，可以将格式化字符串输出到指定文件中
// ● sprintf用法和printf一样，可以将格式化字符串输出到指定字符数组中，注意避免缓冲区溢出
// ● snprintf用法sprintf基本一致，参数2可以指定缓冲区大小，避免缓冲区溢出更安全
// 注：sprint_s用法和snprintf一致（仅限Windows系统）


#include <stdio.h>

int main(){
    int year = 2026, month = 8, day = 22;
    FILE *fp = fopen("test.txt", "w");
    if(fp==NULL){
        perror("");
        return -1;
    }

    char buf[1024] = {0};
    fprintf(fp, "%d-%02d-%02d\n", year, month, day);

    //数据输出到buf中
    snprintf(buf, 1024, "%d-%02d-%02d\n", year, month, day);
    printf("buf: %s\n", buf);
    fclose(fp);

    return 0;
}

