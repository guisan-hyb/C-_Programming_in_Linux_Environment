// 2个进程通过消息队列轮流将键盘输入的字符串发送给对方，接收并打印对方发送的消息

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
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
        perror("msgid: ");
        exit(-1);
    }

    while(1){
        buf.mtype = TypeB;
        printf("input> ");
        fgets(buf.mtext, LEN, stdin);
        msgsnd(msgid, &buf, LEN, 0);

        if(msgrcv(msgid,&buf,LEN,TypeA,0) < 0){
            perror("msgcv: ");
            exit(-1);
        }

        printf("recv from ClienB: %s", buf.mtext);
    }

    return 0;
}