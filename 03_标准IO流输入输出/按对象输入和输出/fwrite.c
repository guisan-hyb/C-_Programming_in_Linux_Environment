// fwrite函数可以从任意类型的文件中读取数据
// 原型：size_t fwrite(void *ptr, size_t size, size_t n, FILE *fp);
// ● 参数1： 缓冲区指针，被输出的数据
// ● 参数2： 1个对象的大小
// ● 参数3： 本次输出多少个对象
// ● 参数4： 文件流指针

// 工作流程： 
// 1. 输出内容到指定的文件流指针 
// 2. 本次调用输出n个对象，每个对象大小为size

#include <stdio.h>
int main(){
    FILE *fp = fopen("outfile", "w");
    if(fp==NULL) {
        perror("打开文件失败: ");
        return -1;
    }

    struct Student
    {
        int no;
        char name[10];
        float score;
    };

    struct Student s[] = {{1, "haha", 99}, {2, "lala", 99}};
    fwrite(s, sizeof(struct Student), 2, fp);
    fclose(fp);

    return 0;
}