#ifndef SERVER_CONF_H__
#define SERVER_CONF_H__

#define DEFAULT_MEDIADIR   "/var/media"
#define DEFAULT_IFNAME     "eth0"

enum {
    RUN_DAEMON = 1,
    RUN_FOREGROUND
};

struct server_conf_st {
    char *rcvport;
    char *mgroup;
    char *media_dir;
    char runmode;
    char *ifname; /*传输数据的网卡设备*/
};

extern struct server_conf_st server_conf;

#endif
