// 2个进程通过消息队列轮流将键盘输入的字符串发送给对方，接收并打印对方发送的消息

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdlib.h>
#include <string.h>

typedef struct MSG
{
    long mtype;
    char mtext[64];
} MSG;

#define LEN (sizeof(MSG) - sizeof(long))
#define TypeA 100
#define TypeB 200


int main(){
    key_t key;
    int msgid;
    MSG buf;

    if((key = ftok(".",'q')) == -1){
        perror("ftok: ");
        exit(-1);
    }

    if((msgid = msgget(key,IPC_CREAT|0666)) < 0){
        perror("msgget: ");
        exit(-1);
    }

    while(1){
        if(msgrcv(msgid,&buf,LEN,TypeB,0) < 0){
            perror("msgcv: ");
            exit(-1);
        }

        printf("recv from ClientA: %s", buf.mtext);

        buf.mtype = TypeA;
        printf("input> ");
        fgets(buf.mtext, LEN, stdin);
        msgsnd(msgid, &buf, LEN, 0);
    }

    return 0;
}