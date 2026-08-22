// rewind函数可以将流重置为开头 
//  long rewind(FILE *stream);
// 效果等同于
//  fseek(fp, 0, SEEK_SET);

#include <stdio.h>

int main(){
    FILE *fp = fopen("test.txt", "r");
    if(fp==NULL){
        perror("打开文件失败");
        return -1;
    }

    char line[1024] = {0};
    fgets(line, 1024, fp);
    printf("%s", line);

    rewind(fp);

    fgets(line, 1024, fp);
    printf("%s", line);

    fclose(fp);

    return 0;
}

