#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>

void commandRun(const char* command);
void error_die(const char* error);

int main(void)
{
    char command[100];
    
    while (1)
    {
        // 接受用户输入的命令并存储
        printf("Enter a command: ");
        if (fgets(command, sizeof(command), stdin) != NULL)
        {
            command[strcspn(command, "\n")] = 0;
            commandRun(command);
        } else
        {
            printf("Error when get commands");
            break;
        }
    }

    return 0;
}

void commandRun(const char* command)
{
    int command_out[2];
    int command_in[2];
    pid_t pid;
    char c;
    int status;

    // 创建管道
    if (pipe(command_out) < 0)
    {
        printf("Error when create pipe");
        exit(1);
    }
    if (pipe(command_in) < 0)
    {
        printf("Error when create pipe");
        exit(1);
    }

    if ((pid = fork()) < 0) // 创建子进程
    {
        perror("Error when fork");
        exit(1);
    } else if (pid == 0) // 在子进程中运行用户命令, 并将运行结果通过管道流到主进程中
    {
        dup2(command_out[1], STDOUT_FILENO); // 把 标准输出 重定向至 command_out 的写入端，以待主进程读取
        dup2(command_in[0], STDIN_FILENO); // 把 标准输入 重定向至 command_in 的读取端, 以等待从主进程流进的数据
        close(command_out[0]);
        close(command_in[1]); // 关闭不必要的管道端

        execlp("sh", "sh", "-c", command, (char *) NULL);
        // 如果 execlp 返回，说明出现了错误
        perror("Error when executing command");
        exit(1);
    } else // 在主进程中的操作
    {
        close(command_out[1]);
        close(command_in[0]);

        while (read(command_out[0], &c, 1) > 0)
            write(STDOUT_FILENO, &c, 1);

        close(command_out[0]);
        close(command_in[1]);

        waitpid(pid, &status, 0); // 等待子进程结束，并获取其退出状态。
    }

}

void error_die(const char* error)
{
    // 调用 perror() 将sc错误信息输出到标准错误流
    perror(error);
    exit(1); // 异常退出
}