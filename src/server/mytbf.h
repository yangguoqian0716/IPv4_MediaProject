#ifndef MYTBF_H
#define MYTBF_H

#define  MYTBF_MAX 1024

typedef void mytbf_t;

mytbf_t *mytbf_init(int cps,int burst);/*速率和上线*/
int mytbf_fetchtoken(mytbf_t *,int i);
int mytbf_returntokrn(mytbf_t *,int i);
int mytbf_desdroy(mytbf_t *);

#endif
