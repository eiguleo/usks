#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/select.h>

int main()
{
    fd_set readfds;
    // struct timeval tv;
    int ret;
    char buf[100];
    int i=0;
    
    while (1)
    {
        printf("=> round %d\n", i);
        FD_ZERO(&readfds); // 清空集合
        FD_SET(STDIN_FILENO, &readfds); // 将标准输入加入集合

        // 设置超时时间
        // tv.tv_sec = 5; // 5秒
        // tv.tv_usec = 0;

        // 使用select等待标准输入可读或者超时
        printf("=> before select \n");
        ret = select(STDIN_FILENO + 1, &readfds, NULL, NULL, NULL);
        printf("=> after select \n");        
        if (ret == -1)
        {
            perror("select");
            exit(EXIT_FAILURE);
        }
        // else if (ret == 0)
        // {
        //     printf("Select timeout, 5 seconds have passed\n");
        // }
        else
        {
            if (FD_ISSET(STDIN_FILENO, &readfds)) // 检查标准输入是否可读
            {
                // 从标准输入读取数据
                int n = read(STDIN_FILENO, buf, sizeof(buf) - 1);
                if (n > 0)
                {
                    buf[n] = '\0'; // 确保字符串结尾
                    printf("You entered: %s\n", buf);
                }
            }
        }
        i++;
    }

    return 0;
}