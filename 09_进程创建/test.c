// 在主进程里开辟子进程，主进程打印“我是主进程”，
// 子进程打印“我是子进程”，然后主进程结束子进程，最后主进程自己退出

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

int main()
{
    printf("开始创建子进程...\n");

    // 1. 创建子进程
    pid_t pid = fork();

    if (pid < 0)
    {
        // fork 失败
        perror("fork error");
        exit(EXIT_FAILURE);
    }
    else if (pid == 0)
    {
        // ================= 子进程逻辑 =================
        printf("我是子进程 (PID: %d)，父进程 PID: %d\n", getpid(), getppid());

        // 子进程需要保持运行，等待父进程来结束它。
        // 这里用 while(1) 模拟子进程在持续干活。
        // 使用 sleep(1) 防止疯狂打印刷屏，并让出 CPU。
        while (1)
        {
            sleep(1);
        }
        // 这行代码正常情况下不会执行，除非收到未捕获的终止信号
        _exit(EXIT_SUCCESS);
    }
    else
    {
        // ================= 父进程逻辑 =================
        printf("我是主进程 (PID: %d)，我创建了子进程 (PID: %d)\n", getpid(), pid);

        // 等待 2 秒，确保子进程有时间打印出它的信息
        sleep(2);

        // 2. 主进程结束子进程
        printf("主进程准备结束子进程...\n");

        // 发送 SIGKILL (9) 信号强制终止子进程
        // 也可以发送 SIGTERM (15)，SIGTERM 允许子进程在退出前做清理工作
        if (kill(pid, SIGKILL) == -1)
        {
            perror("kill error");
        }

        // 3. 回收子进程资源 (防止僵尸进程)
        int status;
        pid_t wait_pid;

        // 阻塞等待指定 PID 的子进程状态改变
        do
        {
            wait_pid = waitpid(pid, &status, 0);
        } while (wait_pid == -1 && errno == EINTR); // 如果被信号中断，则重试

        if (wait_pid == -1)
        {
            perror("waitpid error");
            exit(EXIT_FAILURE);
        }

        // 检查子进程是如何退出的
        if (WIFSIGNALED(status))
        {
            printf("主进程：子进程已被信号 %d (%s) 终止。\n",
                   WTERMSIG(status),
                   strsignal(WTERMSIG(status)));
        }
        else if (WIFEXITED(status))
        {
            printf("主进程：子进程正常退出，退出码: %d\n", WEXITSTATUS(status));
        }

        // 4. 主进程自己退出
        printf("主进程任务完成，准备退出。\n");
        exit(EXIT_SUCCESS);
    }

    return 0;
}

