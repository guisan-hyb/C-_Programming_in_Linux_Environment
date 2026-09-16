#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

int main()
{
    int fd;
    char buf[1024];

    fd = open("myfifo", O_RDONLY);
    if (fd == -1)
    {
        perror("open read error");
        return -1;
    }

    printf("pipe open success, waiting for data...\n");

    int n;
    while ((n = read(fd, buf, sizeof(buf) - 1)) > 0)
    {
        buf[n] = '\0'; // 手动添加结束符
        printf("Received %d bytes. Message: %s", n, buf);
        printf("Message length (without newline): %lu\n", strlen(buf) - 1);
    }

    printf("read returned 0, write end closed. Exiting...\n");
    close(fd);
    return 0;
}