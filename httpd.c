#include <ctype.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>

void commandRun(const char* command);
void error_die(const char* error);
int startup(u_short* port);




int main(void)
{   
    int server_sock = -1;
    u_short port = 4000;
    int client_sock = -1;

    server_sock = startup(&port);
    printf("Server running on port %d 👌\n\n", port);




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

int startup(u_short* port)
{
    // 1. 创建套接字描述符
    // 2. 为套接字设置选项
    // 3. 将套接字与属性绑定
    // 4. 处理端口为0的情况下动态分配端口
    // 5. 将套接字设置为监听状态


    int server = 0;
    int on = 1;
    struct sockaddr_in sock_config;

    // 1. 用socket函数创建套接字描述符
    server = socket(AF_INET, SOCK_STREAM, 0);
    if (server == -1)
        error_die("socket");
    // 2. 为套接字设置选项
    if (setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
        error_die("setsockopt");
    // 3. 将套接字与属性绑定
    memset(&sock_config, 0, sizeof(sock_config)); // 把 sock_config 结构体清零以待后续操作
    sock_config.sin_family = AF_INET; // 设置协议族为 IPv4
    sock_config.sin_port = htons(*port); // 设置端口
    sock_config.sin_addr.s_addr = htonl(INADDR_ANY); // 设置IP地址
    if (bind(server, (struct sockaddr*)&sock_config, sizeof(sock_config)) < 0)
        error_die("bind");
    // 4. 处理端口为0的情况下动态分配端口
    if (*port == 0)
    {
        socklen_t sock_config_len = sizeof(sock_config);
        // 判断sock是否创建成功
        if (getsockname(server, (struct sockaddr*)&sock_config, &sock_config_len) == -1)
            error_die("getsock");
        // 如果获取 sock 信息没有报错，则可以从sock信息结构体变量name中拿到想要的值
        *port = ntohs(sock_config.sin_port);
    }
    // 5. 将套接字设置为监听状态
    if (listen(server, 5) < 0)
        error_die("listen");

    return server;
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