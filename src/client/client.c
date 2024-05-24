#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <net/if.h>

#include "client.h"
#include "proto.h"

struct client_conf_st client_conf = {
        .rcvport = DEFAULT_RCVPORT,
        .mgroup = DEFAULT_MGROUP,
        .player_cmd = DEFAULT_PLAYERCMD
};

static void printhelp(void)
{
    printf("Usage: %s [options]\n", program_invocation_short_name);
    printf("Options:\n");
    printf("\t-M --mgroup 指定多播组\n");
    printf("\t-P --port 指定接收端口\n");
    printf("\t-p --player 指定播放器\n");
    printf("\t-H --help 显示帮助\n");
}

static ssize_t writen(int fd, char *buf, size_t len)
{
    size_t pos = 0;
    while (len > 0)
    {
        ssize_t ret = write(fd, buf + pos, len);
        if (ret < 0)
        {
            if (errno == EINTR)
                continue;
            perror("write()");
            exit(1);
        }
        pos += ret;
        len -= ret;
    }
    return pos;
}

int main(int argc, char **argv)
{
    int c;
    int sd;
    int val;
    int pd[2];
    pid_t pid;
    int chosenid;
    int index = 0;
    socklen_t serveraddr_len, raddr_len;
    struct ip_mreqn mreq;
    struct sockaddr_in laddr, serveraddr, raddr;
    struct option argarr[] = {
            {"port", 1, NULL, 'P'},
            {"mgroup", 1, NULL, 'M'},
            {"player", 1, NULL, 'p'},
            {"help", 0, NULL, 'H'},
            {NULL, 0, NULL, 0}
    };

    while (1)
    {
        c = getopt_long(argc, argv, "P:M:p:H", argarr, &index);
        if (c < 0)
        {
            break;
        }
        switch (c)
        {
            case 'P':
                client_conf.rcvport = optarg;
                break;
            case 'M':
                client_conf.mgroup = optarg;
                break;
            case 'p':
                client_conf.player_cmd = optarg;
                break;
            case 'H':
                printhelp();
                exit(0);
                break;
            default:
                abort();
                exit(1);
                break;
        }
    }

    sd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sd < 0)
    {
        perror("socket()");
        exit(1);
    }

    inet_pton(AF_INET, client_conf.mgroup, &mreq.imr_multiaddr);
    inet_pton(AF_INET, "0.0.0.0", &mreq.imr_address);
    mreq.imr_ifindex = if_nametoindex("eth0");
    if (setsockopt(sd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
    {
        perror("setsockopt()");
        exit(1);
    }

    val = 1;
    if (setsockopt(sd, IPPROTO_IP, IP_MULTICAST_LOOP, &val, sizeof(val)) < 0)
    {
        perror("setsockopt()");
        exit(1);
    }

    laddr.sin_family = AF_INET;
    laddr.sin_port = htons(atoi(client_conf.rcvport));
    inet_pton(AF_INET, "0.0.0.0", &laddr.sin_addr);
    if (bind(sd, (struct sockaddr *)&laddr, sizeof(laddr)) < 0)
    {
        perror("bind()");
        exit(1);
    }

    if (pipe(pd) < 0)
    {
        perror("pipe()");
        exit(1);
    }

    pid = fork();
    if (pid < 0)
    {
        perror("fork()");
        exit(1);
    }

    if (pid == 0)
    {
        // 子进程：调用解码器
        close(sd);
        close(pd[1]);
        dup2(pd[0], 0);
        if (pd[0] > 0)
        {
            close(pd[0]);
        }
        execl("/bin/sh", "sh", "-c", client_conf.player_cmd, NULL);
        perror("execl()");
        exit(1);
    }
    else
    {
        // 父进程：从网络收包，通过管道发送给子进程
        struct msg_list_st *msg_list;
        msg_list = malloc(MSG_LIST_MAX);
        if (msg_list == NULL)
        {
            perror("malloc()");
            exit(1);
        }

        while (1)
        {
            ssize_t len = recvfrom(sd, msg_list, MSG_LIST_MAX, 0, (struct sockaddr *)&serveraddr, &serveraddr_len);
            if (len < sizeof(struct msg_list_st))
            {
                fprintf(stderr, "message is too short\n");
                continue;
            }
            if (msg_list->chnid != LISTCHNID)
            {
                fprintf(stderr, "chnid is not match!\n");
                continue;
            }
            break;
        }

        struct msg_listentry_st *pos;
        for (pos = msg_list->entry; (char *)pos < ((char *)msg_list + len); pos = (struct msg_listentry_st *)((char *)pos + ntohs(pos->len)))
        {
            printf("channel %d: %s\n", pos->chnid, pos->desc);
        }

        while (1)
        {
            int ret = scanf("%d", &chosenid);
            if (ret != 1)
                exit(1);
            if (chosenid >= MINCHNID && chosenid <= MAXCHNID)
                break;
        }

        struct msg_channel_st *msg_channel;
        msg_channel = malloc(MSG_CHANNEL_MAX);
        if (msg_channel == NULL)
        {
            perror("malloc()");
            exit(1);
        }

        while (1)
        {
            ssize_t len = recvfrom(sd, msg_channel, MSG_CHANNEL_MAX, 0, (struct sockaddr *)&raddr, &raddr_len);
            if (raddr.sin_addr.s_addr != serveraddr.sin_addr.s_addr || raddr.sin_port != serveraddr.sin_port)
            {
                fprintf(stderr, "Ignore: address not match!\n");
                continue;
            }

            if (len < sizeof(struct msg_channel_st))
            {
                fprintf(stderr, "Ignore: message is too short\n");
                continue;
            }

            if (msg_channel->chnid == chosenid)
            {
                fprintf(stdout, "accepted msg: %d received\n", msg_channel->chnid);
                if (writen(pd[1], (char *)msg_channel + sizeof(chnid_t), len - sizeof(chnid_t)) < 0)
                    exit(1);
            }
        }

        free(msg_list);
        free(msg_channel);
        close(sd);
    }

    return 0;
}
