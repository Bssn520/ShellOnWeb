/* ShellOnWeb: 在浏览器页面执行shell命令的简单工具
 * 
 * 最近阅读了Tinyhttpd的c语言源码，本着学习的目的，有了这个项目，
 * 根据 Tinyhttpd 源码进行编写，对部分函数进行了优化，
 * 添加了大量注释，方便新同学学习。
*/

#include <ctype.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/stat.h>

#define SERVER_STRING "Server: ShellOnWeb/0.1\r\n"

void commandRun(int client, char* command);
void error_die(const char* error);
int startup(u_short* port);
void accept_request(void *arg);
int get_line(int sock, char *buf, int size);
char* get_body(int client, char** body);
void serve_file(int client, const char *filename);
void headers(int client);
void cat(int client, FILE *resource);
void not_found(int client);


int main(void)
{   
    int server_sock = -1;
    u_short port = 4000; // 可在此处自定义未被占用的端口号
    int client_sock = -1;
    struct sockaddr_in client_config; // client_config 用于存储客户端的地址信息。
    socklen_t  client_config_len = sizeof(client_config);
    pthread_t newthread; // 定义了一个名为newthread的线程标识符变量，其类型为pthread_t

    server_sock = startup(&port);
    printf("Server running on port %d 👌\n\n", port);
    
    while (1)
    {
        client_sock = accept(server_sock, (struct sockaddr*)&client_config, &client_config_len);
        if (client_sock == -1)
            error_die("accept");
        
        if (pthread_create(&newthread, NULL, (void*)accept_request, (void*)(intptr_t)client_sock) != 0)
            error_die("pthread create");
    }

    return 0;
}


/* startup()函数 用于初始化server服务
 * 1. 创建套接字描述符
 * 2. 为套接字设置选项
 * 3. 将套接字与属性绑定
 * 4. 处理端口为0的情况下动态分配端口
 * 5. 将套接字设置为监听状态
*/
int startup(u_short* port)
{
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


/* 接收客户端发来的请求
 * 1. get_line() 获取 request 的第一行, 判断 GET or POST
 * 2. 是 GET 的话,一律使用 server_file() 返回 "src/index.html" 文件
 * 3. 如果是 POST 的话, 取出请求体内的 Content-Length 字段的值，进一步取出 body
 * 4. 拿到 body 后，传给 commandRun(client, body)
*/
void accept_request(void *arg)
{
    int client = (intptr_t)arg;
    char buf[1024];
    size_t numchars;
    char method[255];
    char url[255];
    char path[255];
    struct stat st;

    numchars = get_line(client, buf, sizeof(buf));

    int i = 0, j = 0;
    while (!isspace((int)buf[i]) && (i < sizeof(method) - 1))
    {
        method[i] = buf[i];
        i++;
    }
    j = i; // 记录当前位置

    // 至此拿到 method, 开始分方法处理

    // 处理请求既不是GET又不是POST的情况
    if (strcasecmp(method, "GET") && strcasecmp(method, "POST"))
    {   // if 判断中，非 0 值都为真; 所以只有当 method 既不是 GET 又不是 POST, 才会执行这段代码
        printf("Unsupport methods.");
    }

    // 处理请求为 POST 的情况
    if (strcasecmp(method, "POST") == 0)
    {
        char* body = NULL;

        char* command  = get_body(client, &body);
        printf("command: %s\n", command); // command

        commandRun(client, command);
        free(body); // 释放 body 指针
    }

    // 处理请求为 GET 的情况
    i = 0;
    // 判断如果buf[j]为空格 则不进行赋值操作
    while (isspace((int)buf[j]) && (j < numchars))
        j++;
    // 接着从buf中读取 url 部分到 url字符串
    while(!isspace((int)buf[j]) && (i < sizeof(url) - 1) && (j < numchars))
    {
        url[i] = buf[j];
        i++;
        j++;
    }
    url[i] = '\0';

    sprintf(path, "src%s", url); // 拿到文件路径
    if (path[strlen(path) - 1] == '/') // 如果path以 "/" 结尾
        strcat(path, "index.html");
    
    if (stat(path, &st) == -1)
    {
        while ((numchars > 0) && strcmp("\n", buf))
            // 循环读取并丢弃客户端发送的头信息，直到读取到空行
            numchars = get_line(client, buf, sizeof(buf)); // 从客户端读取一行数据到buf中，并更新numchars
        not_found(client);
    }else
    {
        if((st.st_mode & S_IFMT) == S_IFDIR)// 如果要访问的路径是一个文件夹
            strcat(path, "/index.html");

        serve_file(client, path);
    }

    close(client); // 无论文件是否存在，最终都要关闭连接
}


int get_line(int sock, char *buf, int size)
{
    int i = 0;
    char c = '\0';
    int n;

    while ((i < size - 1) && (c != '\n'))
    {
        n = recv(sock, &c, 1, 0);
        /* DEBUG printf("%02X\n", c); */
        if (n > 0)
        {
            if (c == '\r')
            {
                n = recv(sock, &c, 1, MSG_PEEK);
                /* DEBUG printf("%02X\n", c); */
                if ((n > 0) && (c == '\n'))
                    recv(sock, &c, 1, 0);
                else
                    c = '\n';
            }
            buf[i] = c;
            i++;
        }
        else
            c = '\n';
    }
    buf[i] = '\0';

    return(i);
}


char* get_body(int client, char** body)
{
    char buf[1024];
    size_t numchars;
    size_t content_length = 0;

    // 读取并解析HTTP头部
    numchars = get_line(client, buf, sizeof(buf));
    while (numchars > 0 && strcmp(buf, "\n")) {
        if (strncasecmp(buf, "Content-Length:", 15) == 0) {
            content_length = atoi(&(buf[16]));  // 获取Content-Length
        }
        numchars = get_line(client, buf, sizeof(buf));
    }

    // 读取HTTP body部分
    if (content_length > 0) {
        *body = (char *)malloc(content_length + 1);
        if (*body == NULL) {
            perror("malloc failed");
            return NULL;
        }
        recv(client, *body, content_length, 0);  // 读取指定长度的body
        (*body)[content_length] = '\0';            // 末尾添加空字符

        // 处理 body 提取出 command
        char* command = NULL;
        char* key = "command=";
        command = strstr(*body, key); // 找到 "command=" 的位置; 返回其后的第一个字符的指针
        if (command)
        {
            command += strlen(key);
            return command;
        }
    }

    return NULL;
}


void serve_file(int client, const char *filename)
{
    FILE *resource = NULL;
    int numchars = 1;
    char buf[1024];

    buf[0] = 'A'; buf[1] = '\0';
    while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
        numchars = get_line(client, buf, sizeof(buf));

    resource = fopen(filename, "r");
    if (resource == NULL)
        printf("Can't open file: %s\n", filename);
    else
    {
        headers(client);
        cat(client, resource);
    }
    fclose(resource);
}


/* 对 headers() 函数进行改造，使其将 headers 合并发送
 * 
 *
*/
void headers(int client)
{
    char buf[1024] = "";

    sprintf(buf, "HTTP/1.0 200 OK\r\n%sContent-Type: text/html\r\n\r\n", SERVER_STRING);
    send(client, buf, strlen(buf), 0);
}


/* 修复cat()函数在返回文件内容时 如果文件最后一行不是"\n",则无法返回文件内容最后一行 的错误
 * 
 * 问题描述:
 * fgets() 函数在读取文件时，每次读取一行直到遇到换行符 \n 或达到缓冲区的最大大小。
 * 如果文件中没有换行符，或者最后一行没有换行符，fgets() 可能无法正确读取整行，尤其是文件的最后一行。
 * 例如，如果 2.html 的最后一行（即 </html>）后面没有换行符，
 * 那么 fgets() 在读取这行时可能会出现问题，导致循环在读取到文件末尾之前提前结束。
 * 
 * 解决方法：
 * 将循环条件改为检查 fgets() 的返回值是否为 NULL，而不是检查 feof()。这样可以避免由于 fgets() 读取错误导致的循环终止。
*/
void cat(int client, FILE *resource)
{
    char buf[1024];

    while (fgets((buf), sizeof(buf), resource) != NULL)
    {
        send(client, buf, strlen(buf), 0);
    }
    
}


/* 对 not_found() 函数进行改造，使其将 headers 合并发送
 * 
 *
*/
void not_found(int client)
{
    char buf[1024];

    // 发送HTTP状态行和头部
    sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n%sContent-Type: text/html\r\n\r\n", SERVER_STRING);
    send(client, buf, strlen(buf), 0);

    // 发送HTML正文
    sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n<BODY><P>404 FILE NOT FOUND\r\n</P>\r\n</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}


/* commandRun() 函数负责在子进程中执行用户输入的命令并发回给client
 * 1. pipe() 创建两个管道 command_out command_in
 * 2. fork() 创建子进程
 * 3. 在子进程中: 
 *      (1). 重定向 STDOUT_FILENO 和 STDERR_FILENO 至 command_out[1]; 
 *      (2). 把 STDIN_FILENO 重定向至 command_in[0], 以接收从主进程流进的数据
 *      (3). excelp() 执行命令
 * 4. 在主进程中:
 *      (1). 发送 http headers 给client
 *      (2). 读取 command_out[0], 拿到子进程的运行结果
 *      (3). 使用 send() 将运行结果发还给client
 *      (4). 关闭相关管道、client、子进程
 * 
*/
void commandRun(int client, char* command)
{
    int command_out[2];
    int command_in[2];
    pid_t pid;
    char c;
    int status;
    char buf[1024];

    // 创建管道
    if (pipe(command_out) < 0)
    {
        error_die("Error when create pipe");
    }
    if (pipe(command_in) < 0)
    {
        error_die("Error when create pipe");
    }

    if ((pid = fork()) < 0) // 创建子进程
    {
        error_die("Error when fork");
    } else if (pid == 0) // 在子进程中运行用户命令, 并将运行结果通过管道流到主进程中
    {
        dup2(command_out[1], STDOUT_FILENO); // 把 标准输出 重定向至 command_out 的写入端，以待主进程读取
        dup2(command_out[1], STDERR_FILENO); // 把 标准错误 重定向至 command_out 的写入端，以待主进程读取
        dup2(command_in[0], STDIN_FILENO); // 把 标准输入 重定向至 command_in 的读取端, 以等待从主进程流进的数据
        close(command_out[0]);
        close(command_in[1]); // 关闭不必要的管道端

        execlp("sh", "sh", "-c", command, (char *) NULL); // 使用 sh 执行用户命令
        // 如果 execlp 返回，说明出现了错误
        error_die("Error when executing command");
    } else // 在主进程中的操作
    {
        close(command_out[1]);
        close(command_in[0]);

        headers(client);
        while (read(command_out[0], &c, 1) > 0)
        {
            write(STDOUT_FILENO, &c, 1); // 写到控制台，方便查看
            send(client, &c, 1, 0); // 数据发送给client
        }

        close(command_out[0]);
        close(command_in[1]);

        close(client); // 记得关闭client，非常重要
        waitpid(pid, &status, 0); // 等待子进程结束，并获取其退出状态。
    }
}


void error_die(const char* error)
{
    // 调用 perror() 将sc错误信息输出到标准错误流
    perror(error);
    exit(1); // 异常退出
}