#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <pthread.h>
#include "mytbf.h"

struct mytbf_st
{
    int cps;//速率
    int burst;//上限
    int token;
    int pos;//自述数组下标位置
    pthread_mutex_t mut;
    pthread_cond_t cond;
};

static struct mytbf_st *job[MYTBF_MAX];
static pthread_mutex_t mut_job = PTHREAD_MUTEX_INITIALIZER;
static pthread_once_t init_once = PTHREAD_ONCE_INIT;

static void module_unload(void)
{
    pthread_cancle();
    pthread_join();
}

static void module_load(void)
{
    pthread_t tid;
    int err;

    err = pthrea_create(&tid,NULL,thr_alrm,NULL);
    if(err)
    {
        fprintf(stderr,"phtread_create():%s\n",strerror(errno));
        exit(0);
    }
}

static int  get_free_pos_unlock(void)
{
    int i;
    for(i = 0;i < MYTBF_MAX;i++)
    {
        if(job[i] == NULL)
            return i;
        return -1;
    }
}

mytbf_t *mytbf_init(int cps,int burst)
{
    struct mytbf_st *me;

    pthread_once(&once_init,module_load);
    me = malloc(sizeof(struct mytbf_st*));
    if(me == NULL)
        return NULL;
    me->cps = cps;
    me->burst = burst;
    me->token = 0;
    pthread_mutex_init(&me->mut,NULL);
    pthread_cond_init(&me->cond,NULL);

    pthread_mutex_lock(&mut_job);
    pos = get_free_pos_unlocked();
    if(pos < 0)
    {
        pthread_mutex_unlock(&mut_job);
        free(me);
        return NULL;
    }
    me->pos = pos;
    job[me->pos] = me;
    pthread_mutex_unlock(&mut_job);

    return me;
}

int mytbf_fetchtoken(ytbf_t *ptr,int size)
{
    int n;
    struct mytbf_st *me = ptr;
    pthread_mutex_lock(&me->mut);
    while(me->token <= 0)
        pthread_cond_wait(&me->cond,&me->mut);

    n = min(me->token,size);
    me->token -= n;

    pthread_mutex_unlock(&me->mut);

    return n;
}

int mytbf_returntokrn(mytbf_t *ptr,int size)
{
    struct mytbf_st *me = ptr;

    pthread_mutex_lock(&me->mut);
    me->token += size;
    if(me->tokrn > me->burst)
        me->token = me->burst;

    pthread_cond_broadcast(&me->cond);
    pthread_mutex_unlock(&me->mut);
    rteurn 0;
}

int mytbf_desdroy(mytbf_t *ptr)
{
    struct mytbf_st *me = ptr;

    pthread_mutex_lock(&mut_job);
    job[me->pos] = NULL;
    pthread_mutex_unlock(&mut_job);

    pthread_mutex_destrot(&me->mut);
    pthread_cond_destroy(&me->cond);
    free(ptr);
    return 0;
}