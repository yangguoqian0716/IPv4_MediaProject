# 函数解释
### htons 
将主机字节序的端口号转换为网络字节序
### inet_pton
将字符串形式的 IP 地址转换为网络字节序的数值形式
### getopt_long & getopt
- getopt_long:
  ```c
  #include <stdio.h>
  #include <unistd.h>
  
  int main(int argc, char *argv[]) {
      int opt;
      while ((opt = getopt(argc, argv, "ab:c")) != -1) {
          switch (opt) {
              case 'a':
                  printf("Option a\n");
                  break;
              case 'b':
                  printf("Option b with value %s\n", optarg);
                  break;
              case 'c':
                  printf("Option c\n");
                  break;
              default:
                  fprintf(stderr, "Usage: %s [-a] [-b value] [-c]\n", argv[0]);
                  return 1;
          }
      }
      return 0;
  }

- getopt:是一个用于解析短选项的函数。
  ```c
  #include <stdio.h>
  #include <getopt.h>
  
  int main(int argc, char *argv[]) {
      int opt;
      int option_index = 0;
      struct option long_options[] = {
          {"add",     no_argument,       0,  'a' },
          {"append",  no_argument,       0,  'b' },
          {"delete",  required_argument, 0,  'd' },
          {"verbose", no_argument,       0,  'v' },
          {"create",  required_argument, 0,  'c' },
          {"file",    required_argument, 0,  'f' },
          {0,         0,                 0,  0 }
      };
  
      while ((opt = getopt_long(argc, argv, "ab:d:v:c:f:", long_options, &option_index)) != -1) {
          switch (opt) {
              case 'a':
                  printf("Option add\n");
                  break;
              case 'b':
                  printf("Option append\n");
                  break;
              case 'd':
                  printf("Option delete with value %s\n", optarg);
                  break;
              case 'v':
                  printf("Option verbose\n");
                  break;
              case 'c':
                  printf("Option create with value %s\n", optarg);
                  break;
              case 'f':
                  printf("Option file with value %s\n", optarg);
                  break;
              default:
                  fprintf(stderr, "Usage: %s [--add] [--append] [--delete value] [--verbose] [--create value] [--file value]\n", argv[0]);
                  return 1;
          }
      }
      return 0;
  }

一个用于解析命令行选项的函数,提供了一种处理长选项（即多个字符的选项）的方法.
### if_nametoindex
一个用于将网络接口名称转换为对应索引的函数
### int execl(const char *path, const char *arg, ..., NULL);
- path：要执行的程序的路径。
- arg：这个程序的第一个参数，通常是程序名。
- 后续的参数是传递给该程序的命令行参数，最后一个参数必须是NULL，以表示参数列表的结束。
### Makefile
- CFLAGS 是一个变量，用于指定传递给 C 编译器的选项。
- -I 是包含目录选项，可以指定多个包含目录，用于查找头文件。
- -pthread：这个选项启用了 POSIX 线程（pthread）库的支持。它告诉编译器在编译时使用 pthread 库，以支持多线程编程。
- LDFLAGS 是一个变量，用于指定传递给链接器的选项。

# Linux常用命令
### ps ax -L
- 显示系统中所有的进程（包括与终端无关的进程和其他用户的进程）。
- 显示每个进程的所有线程。
### ps axj
- 显示系统中所有的进程（包括与终端无关的进程和其他用户的进程）。
- 使用作业控制格式显示进程信息。

# 协议封装
### bind问题 
- 该项目中，服务器端可以省略bind函数，直接使用操作系统分配的临时端口。客户端需要调用bind函数，绑定到特定的IP地址和端口号，以便接收服务器发送的数据包。
- 通常情况下：
  1. 被动端（服务器端）：服务器需要绑定一个特定的IP地址和端口号，以便能够接收来自客户端的连接请求。因此，服务器端不能省略bind函数。bind函数将服务器的套接字绑定到一个具体的网络地址（IP地址和端口号）上，使服务器能够监听并接受客户端的连接。
  2. 主动端（客户端）：客户端通常不需要绑定到特定的端口，它会自动选择一个临时的端口进行通信。因此，客户端通常可以省略bind函数。客户端只需要使用connect函数连接到服务器的IP地址和端口号即可。
# C端
### client端任务
1. 分析命令行（长格式）
2. 初始化socket
3. 创建父子进程
4. 父进程
  - 接收网络数据
  - 先接收节目单
  - 选择频道
  - 接收频道包
  - 写入数据到管道
5. 子进程
  - 从挂到读取数据
  - 调用execl函数，调用合适的解码器播放数据信息

### 通过socket网络套接字与server端进行通信，父进程接收网络传输来的数据包，先接收节目单，根据用户需求选择频道，然后接收相应的频道包，然后通过管道，将接收到的数据发送给子进程，子进程调用各种合适的解码器来解析数据。
- 进程间通信：socket、pipe
  - socket采用ipv4协议，以报式套接字的形式传输数据；
  - pipe
- 进程关系：父子进程
- 进程控制：函数execl
# S端
### server端任务
1. 命令行分析(短格式)
2. 守护进程的实现
3. socket初始化
4. 获取频道信息
5. 创建节目单线程
6. 创建频道线程

### server端作守护进程运行，将其输入输出出错重定向。

### 守护进程
- 定义： 守护进程是一种特殊的进程，设计用于在系统启动时自动启动并在后台运行，不与用户直接交互。它们通常提供系统级服务和功能。
- 特点：
  1. 父进程是1号
  2. pid、gid、sid相同
  3. 脱离控制终端
  ```c
  #include <stdio.h>
  #include <stdlib.h>
  #include <unistd.h>
  #include <fcntl.h>
  #include <sys/types.h>
  #include <sys/stat.h>
  #include <signal.h>
  
  void create_daemon() {
      pid_t pid;
  
      // 第一步：创建子进程，父进程退出
      pid = fork();
      if (pid < 0) {
          perror("fork");
          exit(EXIT_FAILURE);
      }
      if (pid > 0) {
          exit(EXIT_SUCCESS); // 父进程退出
      }
  
      // 第二步：创建新会话
      if (setsid() < 0) {
          perror("setsid");
          exit(EXIT_FAILURE);
      }
  
      // 第三步：更改工作目录到根目录
      if (chdir("/") < 0) {
          perror("chdir");
          exit(EXIT_FAILURE);
      }
  
      // 第四步：重设文件掩码
      umask(0);
  
      // 第五步：关闭不需要的文件描述符
      close(STDIN_FILENO);
      close(STDOUT_FILENO);
      close(STDERR_FILENO);
  
      // 可选：将标准输入、输出和错误重定向到 /dev/null
      open("/dev/null", O_RDONLY); // 标准输入
      open("/dev/null", O_WRONLY); // 标准输出
      open("/dev/null", O_WRONLY); // 标准错误
  
      // 守护进程的实际工作
      while (1) {
          // 在此执行守护进程的任务
          sleep(30); // 例如，每 30 秒执行一次任务
      }
  }
  int main() {
      create_daemon();
      return 0;
  }
- 守护进程脱离控制终端没有办法关联标准的输入输出出错，通过写系统日志文件，为了能够记录运行状态、调试信息、错误消息和其他重要事件，守护进程通常会将这些信息写入系统日志文件。
- 守护进程由一个信号意外杀死，为了处理这中异常终止，使用sigaction信号处理函数。

### sigaction
- sigaction 函数用于改变信号的处理方式。它提供了更强大和灵活的信号处理功能，相比于早期的 signal 函数，sigaction 可以更精细地控制信号处理行为。
  
  #include <signal.h>
  int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);
- signum：指定要处理的信号。
- act：指向一个 struct sigaction 结构体，用于指定新的信号处理行为。
- oldact：指向一个 struct sigaction 结构体，用于保存旧的信号处理行为。如果不需要保存旧的行为，可以传入 NULL。

### sigaction结构体
    struct sigaction {
      void (*sa_handler)(int);           // 指向信号处理函数的指针，或者使用常量 SIG_DFL, SIG_IGN
      void (*sa_sigaction)(int, siginfo_t *, void *); // 指向信号处理函数的指针，使用 SA_SIGINFO 标志时
      sigset_t sa_mask;                  // 在处理指定信号时，需要屏蔽的信号集合
      int sa_flags;                      // 控制信号处理行为的标志
      void (*sa_restorer)(void);         // 不常用，一般设为 NULL
      };
    
  - sigemptyset 函数用于初始化一个信号集，使其不包含任何信号。
    ```c
    int sigaddset(sigset_t *set, int signum)
     - 函数用于将指定的信号添加到信号集中。
     - sigaddset 函数用于将指定的信号添加到信号集中。
     - 参数 set 是指向一个 sigset_t 类型的指针，表示要操作的信号集。
     - 参数 signum 是要添加到信号集中的信号编号

### 为什么使用sigaction而不使用signal
因为signal是可重入函数，
如果信号处理函数正在执行时再次收到相同的信号，在某些系统中，这会导致信号处理函数的嵌套调用。
这可能引发竞态条件或栈溢出等问题，特别是在信号处理函数中调用了非可重入的函数；
sigaction 提供了 sa_mask，可以指定在处理一个信号时阻塞哪些其他信号，避免信号处理函数被嵌套调用。
例如，可以阻塞正在处理的信号，从而避免了竞态条件。


