//getchar函数，支持从stdin（标准输入流，默认就是键盘）读取一个字符
//原型：int getchar(void); 读取成功返回字符，失败返回EOF（-1）

#include <stdio.h>

int main(){
    char ch;
    ch = getchar();//等价于 fgetc(stdin)
    printf("%c\n",ch);

    return 0;
}

