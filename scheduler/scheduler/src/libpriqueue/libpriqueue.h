/** @file libpriqueue.h
 */

#ifndef LIBPRIQUEUE_H_
#define LIBPRIQUEUE_H_

// node structure for priority queue implemented through linked list
typedef struct _node_t
{
    void *data; //pointer the data
    struct _node_t *next; //pointer to the next node to recurse through list
} node_t;

node_t *new_node(void *ptr); //create new node process
void insert(node_t *cNode, node_t *newNode); //insert node between nodes

/**
  Priqueue Data Structure
*/
typedef struct _priqueue_t
{
    int size; //size of queue
    node_t *head; //pointer to first node
    int (*comp)(const void *, const void *); //pointer to comparing function
} priqueue_t;

void priqueue_init(priqueue_t *q, int (*comparer)(const void *, const void *));

int priqueue_offer(priqueue_t *q, void *ptr);
void *priqueue_peek(priqueue_t *q);
void *priqueue_poll(priqueue_t *q);
void *priqueue_at(priqueue_t *q, int index);
int priqueue_remove(priqueue_t *q, void *ptr);
void *priqueue_remove_at(priqueue_t *q, int index);
int priqueue_size(priqueue_t *q);

void priqueue_destroy(priqueue_t *q);

#endif /* LIBPQUEUE_H_ */
