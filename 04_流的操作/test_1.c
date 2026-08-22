#include <stdio.h>
#include <time.h>
#include <string.h>
#include <unistd.h>

#define PATH "test.txt"

int main(){
    FILE *fp = fopen(PATH, "a+");
    int line_num = 0;
    char line[1024] = {0};

    time_t t;//时间对象
    struct tm tm_result; // struct tm结构存储时间数据
    struct tm *tp;       // tm结构体指针，调用localtime函数后指向上面的tm_result变量

    if(fp==NULL){
        perror("打开文件失败: ");
        return -1;
    }

    while(fgets(line,1024,fp) != NULL){
        // 如果一行数据比1024多，就需要判断末尾字符
        if(strlen(line) && line[strlen(line)-1] == '\n'){
            line_num++;
        }
    }

    //写入
    while(1){
        time(&t);
        tp = localtime_r(&t, &tm_result);
        if (tp == NULL) {
            fprintf(stderr, "无法获取localTime.\n");
            fclose(fp);
            return -1;
        }

        fprintf(fp, "%d, %d-%02d-%02d %02d:%02d:%02d\n", ++line_num, tp->tm_year + 1900,
                tp->tm_mon + 1, tp->tm_mday, tp->tm_hour, tp->tm_min, tp->tm_sec);

        // 默认文件是全缓冲，需要强制刷新
        fflush(fp);
        // 每隔1s刷新
        sleep(1);
    }

    fclose(fp);

    return 0;
}

