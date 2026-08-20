// fread函数可以从任意类型的文件中读取数据
// 原型： size_t fread(void *ptr, size_t size, size_t n, FILE *fp);

// ● 参数1： 缓冲区指针，在调用函数前定义好，用来存入读取的数据
// ● 参数2： 1个对象的大小
// ● 参数3： 本次读取多少个对象
// ● 参数4： 文件流指针

// 工作流程： 
// 1. 读取指定的文件流指针 
// 2. 本次调用读取n个对象，每个对象 
// 3. 大小为size 
// 4. 将读取的内容，存入缓冲区指针

#include <stdio.h>

int main(){
    char buf[5];//缓冲区
    int bytes_read = 0; // 记录fread返回值读取多少对象
    FILE *fp = fopen("/home/guisan/MyCode/C++_Programming_in_Linux_Environment/03_标准IO流输入输出/test.txt", "r");
    if(fp==NULL){
        perror("打开文件失败: ");
        return -1;
    }

    // 每次读取的对象为1个字节，读取5个对象
    while((bytes_read = fread(buf,1,5,fp)) > 0){
        //.表示输出的精度,后面跟着*表示精度由额外的参数指定
        // 而不是在字符串中直接给出
        printf("%.*s", bytes_read, buf);

        //fwrite(buf, 1, bytes_read, stdout);
    }
    fclose(fp);

    return 0;
}

