#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

int main()
{
    int fd;
    char buf[1024];

    // 修正1：必须以 O_WRONLY 打开
    fd = open("myfifo", O_WRONLY);
    if (fd == -1)
    {
        perror("open write error");
        return -1;
    }

    printf("pipe open success, type message (quit to exit):\n");

    while (1)
    {
        if (fgets(buf, sizeof(buf), stdin) == NULL)
            break;

        if (strncmp(buf, "quit", 4) == 0)
            break;

        int len = strlen(buf);
        write(fd, buf, len);
    }

    close(fd);
    printf("Writer exiting...\n");
    return 0;
}