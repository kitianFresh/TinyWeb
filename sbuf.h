#ifndef __SBUF_H__
#define __SBUF_H__
#include "./rio/wrapper.h"
/* PV sbuf*/
typedef struct {
	int *buf;		/* Buffer array */
	int n;			/* Maximum number of slots */
	int front;		/* buf[(front+1)%n] is first item */
	int rear;		/* buf[rear%n] is last item */
	sem_t mutex;	/* Protects access to buf */
	sem_t slots;	/* Counts available slots */
	sem_t items;	/* Counts available items */
} sbuf_t;
void sbuf_init(sbuf_t *sp, int n);
void sbuf_deinit(sbuf_t *sp);
void sbuf_insert(sbuf_t *sp, int item);
int sbuf_remove(sbuf_t *sp);
#endif
