// 打印指定目录下所有文件名称

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>

int main(int argc, char* argv[]){
    if(argc<2){
        printf("Usage : %s <directory>\n", argv[0]);
        return -1;
    }

    DIR *dir_stream;
    struct dirent *dip;
    if((dir_stream = opendir(argv[1])) == NULL){
        perror("open dir failed: ");
        return -1;
    }

    while((dip = readdir(dir_stream)) != NULL){
        printf("%s\n", dip->d_name);
    }

    closedir(dir_stream);

    return 0;
}

