#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <proto.h>

#include "server_conf.h"
#include "medialib.h"
#include "thr_channel.h"
#include "thr_list.h"

/*
 * -M  指定多播组
 * -P  指定接收端口
 * -F  前台运行模式
 * -D  指定媒体库位置
 * -I  指定网络设备
 * -H  显示帮助信息
 * */

struct server_conf_st server_conf = {.rcvport = DEFAULT_RCVPORT,\
.mgroup = DEFAULT_MGROUP,\
.ifname = DEFAULT_IFNAME,\
.media_dir = DEFAULT_MEDIADIR,\
.runmode = RUN_DAEMON};

static void printhelp(void)
{
    printf("-M  指定多播组\n");
    printf("-P  指定接收端口\n");
    printf("-F  前台运行模式\n");
    printf("-D  指定媒体库位置\n");
    printf("-I  指定网络设备\n");
    printf("-H  显示帮助信息\n");
}

static void daemon_exit(int s)
{
    closelog();
    exit(0);
}

static int daemonize(void)
{
    pid_t pid;
    int fd;

    pid = fork();
    if(pid < 0)
    {
        //perror("fork()");
        syslog(LOG_ERR,"fork(): %s", strerror(errno));
        return -1;
    }
    if(pid > 0)
    {
        exit(0);
    }

    fd = open("/dev/null",O_RDWR);
    if(fd < 0)
    {
        //perror("open()");
        syslog(LOG_WARNING,"open(): %s", strerror(errno));
        return -2;
    }
    else
    {
        // 重定向标准输入输出出错到文件 "/dev/null"中
        dup2(fd, 0);
        dup2(fd, 1);
        dup2(fd, 2);

        if (fd > 2)
        {
            close(fd);
        }
    }
    setsid();// 创建一个新会话，使该子进程成为会话领导者，并脱离控制终端。

    chdir("/"); // 更改工作目录到根目录，避免依赖具体路径
    umask(0); // 置文件模式创建掩码为 0，确保创建的文件和目录具有最大的权限

    return 0;
}

static void socket_init(void)
{
    int server_sd;
    struct ip_mreqn mreq;

    server_sd = socket(AF_INET,SOCK_DGRAM,0);
    if(server_sd < 0)
    {
        syslog(LOG_ERR,"socket():%s",strerror(errno));
        exit(1);
    }

    inet_pton(AF_INET,server_conf.mgroup,&mreq.imr_multiaddr);/*多播地址*/
    inet_pton(AF_INET,"0.0.0.0",&mreq.imr_address);/*本机IP,ANYADDRESS*/
    //在多播场景中，"0.0.0.0" 通常用来表示加入多播组时不关心特定的本地接口。本机的任意地址，即不限定特定的本地 IP 地址。
    //这表示在多播中使用本机的任意网络接口来接收多播流量，而不是指定特定的网络接口。

    mreq.imr_ifindex = if_nametoindex(server_conf.ifname);/*设备索引号*/
    if(setsockopt(server_sd,IPPROTO_IP,IP_MULTICAST_IF,&mreq,sizeof(mreq)) < 0)// 创建多播组
    {
        syslog(LOG_ERR,"setsocketopt(IP_MULTICAST_IF):%s",strerror(errno));
        exit(1);
    }
}

int main(int argc,char **argv)
{
    int c,err,i;
    int list_size;
    struct sigaction sa;
    struct mlib_listentry_st *list;

    sa.sa_handler = daemon_exit;// 信号处理函数
    // 当接收到sa.sa_mask信号集合中的信号时，信号作出daemon_exit处理，来正常终止守护进程。

    sigemptyset(&sa.sa_mask);// 清空信号集
    sigaddset(&sa.sa_mask,SIGINT);// 往信号集中加入信号
    sigaddset(&sa.sa_mask,SIGQUIT);
    sigaddset(&sa.sa_mask,SIGTERM);

    sigaction(SIGTERM, &sa, NULL);// 信号到来时，作出daemon_exit处理，来正常终止守护进程。
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);
    /*打开日志文件*/
    openlog("netradio",LOG_PID|LOG_PERROR,LOG_DAEMON);

    /* 命令行分析 */
    while(1)
    {
        c = getopt(argc,argv,"M:P:FD:I:H");
        if(c < 0)
        {
            break;
        }
        switch(c)
        {
            case 'M':
                server_conf.mgroup = optarg;
                break;
            case 'P':
                server_conf.rcvport = atoi(optarg);
                break;
            case 'F':
                server_conf.runmode = RUN_FOREGROUND;
                break;
            case 'D':
                server_conf.media_dir = optarg;
                break;
            case 'I':
                server_conf.ifname = optarg;
                break;
            case 'H':
                printfhelp();
                break;
            default:
                abort();//调用 abort() 会导致程序向操作系统发送退出信号，终止程序的运行
                exit(0);
        }
    }

    /* 守护进程的实现 */
    if(server_conf.runmode == RUN_DAEMON)
    {
        if(daemonize() != 0)
        {
            exit(1);
        }
    }
    else if(server_conf.runmode == RUN_FOREGROUND)
    {
        printf("run in foreground mode\n");
    }
    else
    {
        //fprintf(stderr,"EINVAL\n");/*报错，参数非法*/
        syslog(LOG_ERR, "EINVAL server_conf.runmode.");
        exit(1);
    }

    /*SOKET初始化*/
    socket_init();
    /*获取频道信息*/
    err = mlib_getchnlist(&list,&list_size);
    if(err)
    {

    }
    /*创建节目单线程*/
    thr_list_create(list,list_size);

    /*创建频道线程*/
    for(i = 0;i<list_size;i++)
    {
        thr_channel_create(list + i);
    }

    syslog(LOG_DEBUG,"%d channel threads created.",i);

    while(1)
        pause();
}
