#include "kernel/types.h"
#include "user/user.h"
#include "kernel/stat.h"

int main(int argc, char *argv[])
{
    // 调用 getprocs 系统调用，返回活动进程数
    int count = getprocs();
    
    // 打印活动进程数
    printf("There are %d active processes.\n", count);
    
    // 程序退出
    exit(0);
}
