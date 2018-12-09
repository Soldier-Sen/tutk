#ifndef _AVAPIs2_FQ_H_
#define _AVAPIs2_FQ_H_

#include <pthread.h>
#include "P2PCam/AVFRAMEINFO.h"

typedef enum 
{
	QUEUE_OP_NOERROR		= 0,
	QUEUE_OP_INEXISTENT		= -1,	// Applying queue operation on a queue that doesn't exist.
	QUEUE_OP_ISOLATED		= -2,	// The nodes under operation have no connection.
	QUEUE_OP_INCONSISTENT	= -3,	// The nodes under operation are not in the same queue.
	QUEUE_OP_INCONSISTENT_R	= -4,	// The nodes under operation are not in the same reversed queue.
	QUEUE_OP_MUTEX_FAIL		= -5,

} ENUM_QUEUE_OP_STATUS;

typedef struct Frame_Node Frame_Node;
typedef struct Frame_Queue Frame_Queue;

struct Frame_Node
{
    char *frame_data;
    FRAMEINFO_t *frame_info;
    int frame_size;
    int frame_no;
    Frame_Node *prev_frame;
    Frame_Node *next_frame;
};

struct Frame_Queue
{
    pthread_mutex_t lock;
    Frame_Node *head;
    Frame_Node *tail;
};

int kalay_queue_init(Frame_Queue *);
int kalay_queue_free(Frame_Queue *);
int kalay_queue_insert(Frame_Queue *, Frame_Node *);
int kalay_queue_remove(Frame_Queue *, Frame_Node *);
int kalay_queue_locate(Frame_Queue *, Frame_Node *);
int kalay_queue_count(Frame_Queue *, Frame_Node *);
unsigned int kalay_queue_duration(Frame_Queue *);
int kalay_queue_check_consistency(Frame_Queue *, Frame_Node *, Frame_Node *);
Frame_Node *kalay_queue_find(Frame_Queue *, Frame_Node *, void *);
Frame_Node *kalay_queue_rfind(Frame_Queue *, Frame_Node *, void *);
Frame_Node *kalay_queue_pop(Frame_Queue *, Frame_Node *, void *);
Frame_Node *kalay_queue_rpop(Frame_Queue *, Frame_Node *, void *);
unsigned int kalay_queue_head_timestamp(Frame_Queue *);
unsigned int kalay_queue_tail_timestamp(Frame_Queue *);

#endif // _AVAPIs2_FQ_H_