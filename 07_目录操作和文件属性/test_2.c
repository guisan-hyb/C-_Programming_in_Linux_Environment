// #include <sys/stat.h>
// int chmod(const char *path, mode_t mode);
// int fchmod(int fd, mode_t mode);

#include <stdio.h>
#include <sys/stat.h>

int main(){
    chmod("test.txt", 0666);

    return 0;
}

