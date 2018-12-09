#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "AVAPIs2_FQ.h"

#define KALAY_QUEUE_REMOVE_INNER(_queue, _target_node)\
do {\
	if (_target_node) {\
		if(!_target_node->next_frame && !_target_node->prev_frame) {\
			_queue->tail = _queue->head = NULL;\
		} else if (!_target_node->next_frame) {\
			_queue->tail = _target_node->prev_frame;\
			_target_node->prev_frame->next_frame = NULL;\
		} else if (!_target_node->prev_frame) {\
			_queue->head = _target_node->next_frame;\
			_target_node->next_frame->prev_frame = NULL;\
		} else {\
			_target_node->prev_frame->next_frame = _target_node->next_frame;\
			_target_node->next_frame->prev_frame = _target_node->prev_frame;\
		}\
		_target_node->next_frame = _target_node->prev_frame = NULL;\
	}\
} while(0);

#define KALAY_QUEUE_FIND_INNER(_node, _search_target)\
do {\
	unsigned int _target;\
	int _count = 1;\
	if (strncmp((char *)_search_target, "#", 1) == 0) {\
		sscanf((char *)_search_target, "#%3d", &_target);\
		while (_node) {\
			if (_count == _target) {\
				break;\
			} else {\
				_node = _node->next_frame;\
				_count++;\
			}\
		}\
	} else if (strncmp((char *)_search_target, "I", 1) == 0) {\
		while (_node) {\
			if (_node->frame_info->flags == IPC_FRAME_FLAG_IFRAME) {\
				break;\
			} else {\
				_node = _node->next_frame;\
			}\
		}\
	} else if (strncmp((char *)_search_target, "P", 1) == 0) {\
		while (_node) {\
			if (_node->frame_info->flags == IPC_FRAME_FLAG_PBFRAME) {\
				break;\
			} else {\
				_node = _node->next_frame;\
			}\
		}\
	} else if (strncmp((char *)_search_target, "%", 1) == 0) {\
		sscanf((char *)_search_target, "%%%u", &_target);\
		while (_node) {\
			if (_node->frame_info->timestamp >= _target)\
				break;\
			else\
				_node = _node->next_frame;\
		}\
	}\
} while(0);

#define KALAY_QUEUE_RFIND_INNER(_node, _search_target)\
do {\
	unsigned int _target;\
	int _count = 1;\
	if (strncmp((char *)_search_target, "#", 1) == 0) {\
		sscanf((char *)_search_target, "#%3d", &_target);\
		while (_node) {\
			if (_count == _target) {\
				break;\
			} else {\
				_node = _node->prev_frame;\
				_count++;\
			}\
		}\
	} else if (strncmp((char *)_search_target, "I", 1) == 0) {\
		while (_node) {\
			if (_node->frame_info->flags == IPC_FRAME_FLAG_IFRAME) {\
				break;\
			} else {\
				_node = _node->prev_frame;\
			}\
		}\
	} else if (strncmp((char *)_search_target, "P", 1) == 0) {\
		while (_node) {\
			if (_node->frame_info->flags == IPC_FRAME_FLAG_PBFRAME) {\
				break;\
			} else {\
				_node = _node->prev_frame;\
			}\
		}\
	} else if (strncmp((char *)_search_target, "%", 1) == 0) {\
		sscanf((char *)_search_target, "%%%u", &_target);\
		while (_node) {\
			if (_node->frame_info->timestamp <= _target)\
				break;\
			else\
				_node = _node->prev_frame;\
		}\
	}\
} while(0);

int kalay_queue_init(Frame_Queue *queue) {

    queue = (Frame_Queue *) malloc(sizeof(Frame_Queue));

    queue->head = NULL;
    queue->tail = NULL;

	int ret = pthread_mutex_init(&queue->lock, NULL);

	if (ret != 0) {
        printf("Mutex_init for Kalay Queue failed.\n");
        return QUEUE_OP_MUTEX_FAIL;
    }

    return ret;
}

int kalay_queue_free(Frame_Queue *queue) {

	if (!queue->head) {
		queue = NULL;
		free(queue);
		return 0;
	}

	Frame_Node *node = queue->head;
	Frame_Node *next;

	while (node->next_frame) {
		next = node->next_frame;
		node = NULL;
		free(node);
		node = next;
	}

	return 0;
}

int kalay_queue_insert(Frame_Queue *queue, Frame_Node *target_node) {
	if (queue->head && queue->tail) {
		if (kalay_queue_check_consistency(queue, queue->head, queue->tail) != 0) {
			printf("The queue with the given head and tail information doesn't exist. Abort.\n");
			return QUEUE_OP_INEXISTENT;
		}
	}

	Frame_Node *node;
	char timestamp[32];

	pthread_mutex_lock(&queue->lock);

	if (!queue->tail) {
		queue->tail = target_node;
		queue->head = target_node;
	} else if (queue->tail == queue->head) {
		if (queue->tail->frame_info->timestamp <= target_node->frame_info->timestamp)
			queue->tail = target_node;
		else
			queue->head = target_node;
		queue->head->next_frame = queue->tail;
		queue->tail->prev_frame = queue->head;
	} else {
		sprintf(timestamp, "%%%u", target_node->frame_info->timestamp);
		node = queue->tail;
		KALAY_QUEUE_RFIND_INNER(node, timestamp);

		if (!node) {
			target_node->next_frame = queue->head;
			queue->head->prev_frame = target_node;
			queue->head = target_node;
		} else {
			if (node == queue->tail) {
				queue->tail = target_node;
			} else {
				node->next_frame->prev_frame = target_node;
				target_node->next_frame = node->next_frame;
			}
			node->next_frame = target_node;
			target_node->prev_frame = node;
		}
	}
	pthread_mutex_unlock(&queue->lock);

	return QUEUE_OP_NOERROR;
}

int kalay_queue_remove(Frame_Queue *queue, Frame_Node *target_node) {

	if (kalay_queue_check_consistency(queue, queue->head, target_node) != 0) {
		return QUEUE_OP_INEXISTENT;
	}

	pthread_mutex_lock(&queue->lock);

	KALAY_QUEUE_REMOVE_INNER(queue, target_node);

	pthread_mutex_unlock(&queue->lock);

	return QUEUE_OP_NOERROR;
}

int kalay_queue_locate(Frame_Queue *queue, Frame_Node *target_node) {

	if (kalay_queue_check_consistency(queue, queue->head, target_node) != 0) {
		return QUEUE_OP_INEXISTENT;
	}

	Frame_Node *node = target_node;

	int count = 1;

	pthread_mutex_lock(&queue->lock);

	while(node->prev_frame) {
		count++;
		node = node->prev_frame;
	}

	pthread_mutex_unlock(&queue->lock);

	return count;
}

int kalay_queue_count(Frame_Queue *queue, Frame_Node *target_node) {

	if (kalay_queue_check_consistency(queue, queue->head, target_node) != 0) {
		return QUEUE_OP_INEXISTENT;
	}

	Frame_Node *node = target_node;

	int count = 1;

	pthread_mutex_lock(&queue->lock);

	while(node->next_frame) {
		count++;
		node = node->next_frame;
	}

	node = target_node;

	while(node->prev_frame) {
		count++;
		node = node->prev_frame;
	}

	pthread_mutex_unlock(&queue->lock);

	return count;
}

unsigned int kalay_queue_duration(Frame_Queue *queue) {

	unsigned int ret;

	pthread_mutex_lock(&queue->lock);

    if(queue->tail == NULL || queue->head == NULL)
        ret = 0;
    else
    	ret = queue->tail->frame_info->timestamp - queue->head->frame_info->timestamp;

	pthread_mutex_unlock(&queue->lock);

	return ret;
}

int kalay_queue_check_consistency(Frame_Queue *queue, Frame_Node *front_node, Frame_Node *back_node) {
	Frame_Node *node;

	int ret = 3;

	pthread_mutex_lock(&queue->lock);

	// Check whether back_node is in the same queue after front_node.
	node = front_node;
	while (node) {
		if (node == back_node) {
			ret &= 2;
			break;
		} else {
			node = node->next_frame;
		}
	}

	// Check whether front_node is in the same queue before back_node.
	node = back_node;
	while (node) {
		if (node == front_node) {
			ret &= 1;
			break;
		} else {
			node = node->prev_frame;
		}
	}

	switch (ret) {
		case 0:
			break;
		case 1:
			ret = QUEUE_OP_INCONSISTENT;
			break;
		case 2:
			ret = QUEUE_OP_INCONSISTENT_R;
			break;
		case 3:
			ret = QUEUE_OP_ISOLATED;
			break;
	}

	pthread_mutex_unlock(&queue->lock);

	return ret;
}

Frame_Node *kalay_queue_find(Frame_Queue *queue, Frame_Node *start_node, void *search_target) {

	pthread_mutex_lock(&queue->lock);

	Frame_Node *node = start_node;

	KALAY_QUEUE_FIND_INNER(node, search_target);

	pthread_mutex_unlock(&queue->lock);

	return node;
}

Frame_Node *kalay_queue_rfind(Frame_Queue *queue, Frame_Node *start_node, void *search_target) {

	pthread_mutex_lock(&queue->lock);

	Frame_Node *node = start_node;

	KALAY_QUEUE_RFIND_INNER(node, search_target);

	pthread_mutex_unlock(&queue->lock);

	return node;
}

Frame_Node *kalay_queue_pop(Frame_Queue *queue, Frame_Node *start_node, void *search_target) {

	pthread_mutex_lock(&queue->lock);

	Frame_Node *node = start_node;

	KALAY_QUEUE_FIND_INNER(node, search_target);
	KALAY_QUEUE_REMOVE_INNER(queue, node);

	pthread_mutex_unlock(&queue->lock);

	return node;
}

Frame_Node *kalay_queue_rpop(Frame_Queue *queue, Frame_Node *start_node, void *search_target) {

	pthread_mutex_lock(&queue->lock);

	Frame_Node *node = start_node;

	KALAY_QUEUE_RFIND_INNER(node, search_target);
	KALAY_QUEUE_REMOVE_INNER(queue, node);

	pthread_mutex_unlock(&queue->lock);

	return node;
}

unsigned int kalay_queue_head_timestamp(Frame_Queue *queue) {

	unsigned int ret;

	pthread_mutex_lock(&queue->lock);

    if(queue->head == NULL)
        ret = 0;
    else
    	ret = queue->head->frame_info->timestamp;

	pthread_mutex_unlock(&queue->lock);

	return ret;
}

unsigned int kalay_queue_tail_timestamp(Frame_Queue *queue) {

	unsigned int ret;

	pthread_mutex_lock(&queue->lock);

    if(queue->tail == NULL)
        ret = 0;
    else
    	ret = queue->tail->frame_info->timestamp;

	pthread_mutex_unlock(&queue->lock);

	return ret;
}


