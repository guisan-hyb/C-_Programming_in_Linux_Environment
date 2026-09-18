// 父子进程通过System V信号灯同步对共享内存的读写
// ● 父进程从键盘输入字符串到共享内存
// ● 子进程删除字符串中的空格并打印
// ● 父进程输入quit后删除共享内存和信号灯集，程序结束

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/shm.h>


#define N 64
#define READ 0
#define WRITE 1

// 要修改的信号量的联合体
union semun
{
    int val;
    struct semid_ds *buf;
    unsigned short *array;
    struct seminfo *__buf;
};

// 初始化信号量集合
// semid:信号量集合id，s表述数值数组，n表示信号量个数
void init_sem(int semid,int s[],int n){
    int i;
    union semun myun;
    for (int i = 0; i < n;i++){
        myun.val = s[i];
        semctl(semid, i, SETVAL, myun);
    }
}

// pv操作
// semid:信号量集合id，num表示信号量的序号，op表示+1还是-1
void pv(int semid, int num,int op){
    struct sembuf buf;
    buf.sem_num = num;
    buf.sem_op = op;
    buf.sem_flg = 0;
    semop(semid, &buf, 1);
}

int main(){
    int shmid, semid, s[] = {0, 1};
    pid_t pid;
    key_t key;
    char *shmaddr;

    // 获取 key
    if((key = ftok(".",'s')) == 1){
        perror("ftok");
        exit(-1);
    }

    // 生成共享内存id
    if((shmid = shmget(key,N,IPC_CREAT|0666)) < 0){
        perror("shmget");
        exit(-1);
    }

    // 生成信号量集合id
    if((semid = semget(key,2,IPC_CREAT|0666)) < 0){
        perror("semget");
        goto _error1;
    }

    // 初始化信号量集合
    init_sem(semid, s, 2);

    // 映射内存
    if((shmaddr = (char*)shmat(shmid,NULL,0)) == (char*)-1){
        perror("shmat");
        goto _error2;
    }

    // 开辟进程
    if((pid = fork()) < 0){
        perror("fork");
        goto _error2;
    }else if(pid == 0){ // 子进程
        char *p, *q;
        while(1){
            pv(semid, READ, -1);
            p = q = shmaddr;
            while(*q){
                if(*q != ' '){
                    *p = *q;
                    p++;
                }
                q++;
            }
            *p = '\0';
            printf("%s", shmaddr);
            pv(semid, WRITE, 1);
        }
    }else{ // 父进程
        while(1){
            pv(semid, WRITE, -1);
            printf("input> ");
            fgets(shmaddr, N, stdin);
            if(strcmp(shmaddr,"quit\n") == 0){
                break;
            }
            pv(semid, READ, 1);
        }
        kill(pid, SIGUSR1); // 删除子进程
        shmdt(shmaddr);
    }

_error1:
    shmctl(shmid, IPC_RMID, NULL);

_error2:
    semctl(semid, 0, IPC_RMID);

    return 0;
}
