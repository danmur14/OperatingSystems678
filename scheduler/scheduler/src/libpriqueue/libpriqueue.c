/** @file libpriqueue.c
 */

#include <stdlib.h>
#include <stdio.h>

#include "libpriqueue.h"

/**
  Node functions for creating/destroying
*/
//create new node
node_t *new_node(void *ptr)
{
  node_t *new = (node_t *)malloc(sizeof(node_t));
  new->data = ptr;
  new->next = NULL;
  return new;
}

//insert between nodes
void insert(node_t *cNode, node_t *newNode)
{
  node_t *temp = cNode->next;
  cNode->next = newNode;
  newNode->next = temp;
}

/**
  Initializes the priqueue_t data structure.
  
  Assumtions
    - You may assume this function will only be called once per instance of priqueue_t
    - You may assume this function will be the first function called using an instance of priqueue_t.
  @param q a pointer to an instance of the priqueue_t data structure
  @param comparer a function pointer that compares two elements.
  See also @ref comparer-page
 */
void priqueue_init(priqueue_t *q, int (*comparer)(const void *, const void *))
{
  q->size = 0;
  q->head = NULL;
  q->comp = comparer;
}

/**
  Inserts the specified element into this priority queue.

  @param q a pointer to an instance of the priqueue_t data structure
  @param ptr a pointer to the data to be inserted into the priority queue
  @return The zero-based index where ptr is stored in the priority queue, where 0 indicates that ptr was stored at the front of the priority queue.
 */
int priqueue_offer(priqueue_t *q, void *ptr)
{

  if (ptr == NULL)
  {
    return -1;
  }

  //create new node for item
  node_t *node = new_node(ptr);


  //queue is empty or new element is lower priority than head
  if (q->head == NULL)
  {
    node->next = q->head;
    q->head = node;
    q->size++;
    return 0;
  }
  else if(q->comp(node->data, q->head->data) < 0)
  {
    node->next = q->head;
    q->head = node;
    q->size++;
    return 0;
  }
  //not empty and not at head
  else
  {
    node_t *cNode = q->head;
    int i = 0;
    while (i < priqueue_size(q))
    {
      //if at the end of list or lower priority
      if (cNode->next == NULL || q->comp(node->data, cNode->next->data) < 0)
      {
        insert(cNode, node);
        q->size++;
        return i;
      }

      //else move on
      cNode = cNode->next;
      i++;
    }
  }

  //could not insert
  return -1;
}

/**
  Retrieves, but does not remove, the head of this queue, returning NULL if
  this queue is empty.
 
  @param q a pointer to an instance of the priqueue_t data structure
  @return pointer to element at the head of the queue
  @return NULL if the queue is empty
 */
void *priqueue_peek(priqueue_t *q)
{
  if (priqueue_size(q) == 0)
  {
    return NULL;
  }
  else
  {
    return q->head->data;
  }
}

/**
  Retrieves and removes the head of this queue, or NULL if this queue
  is empty.
 
  @param q a pointer to an instance of the priqueue_t data structure
  @return the head of this queue
  @return NULL if this queue is empty
 */
void *priqueue_poll(priqueue_t *q)
{
  if (priqueue_size(q) == 0)
  {
    return NULL;
  }
  //pop
  else
  {
    void *data = q->head->data;
    node_t *temp = q->head->next;
    free(q->head);
    q->head = temp;
    q->size--;
    return data;
  }
}

/**
  Returns the element at the specified position in this list, or NULL if
  the queue does not contain an index'th element.
 
  @param q a pointer to an instance of the priqueue_t data structure
  @param index position of retrieved element
  @return the index'th element in the queue
  @return NULL if the queue does not contain the index'th element
 */
void *priqueue_at(priqueue_t *q, int index)
{
  //index bigger/smaller than queue
  if (index > priqueue_size(q) || index < 0)
  {
    return NULL;
  }
  else
  {
    //traverse until index
    node_t *cNode = q->head;
    int i = 0;
    while (i < index)
    {
      cNode = cNode->next;
      i++;
    }
    return cNode->data;
  }
  return NULL;
}

/**
  Removes all instances of ptr from the queue. 
  
  This function should not use the comparer function, but check if the data contained in each element of the queue is equal (==) to ptr.
 
  @param q a pointer to an instance of the priqueue_t data structure
  @param ptr address of element to be removed
  @return the number of entries removed
 */
int priqueue_remove(priqueue_t *q, void *ptr)
{
  int removed = 0;
  //if head should be removed
  while (priqueue_size(q) > 0 && q->head->data == ptr)
  {
    priqueue_poll(q);
    removed++;
  }

  //while queue is not empty
  if (priqueue_size(q) > 0)
  {
    //traverse
    node_t *cNode = q->head;
    while (cNode->next != NULL)
    {
      //remove if found
      if (cNode->next->data == ptr)
      {
        node_t *temp = cNode->next;
        cNode->next = temp->next;
        free(temp);
        q->size--;
        removed++;
      }
      //else move on
      else
      {
        cNode = cNode->next;
      }
    }
  }

  return removed;
}

/**
  Removes the specified index from the queue, moving later elements up
  a spot in the queue to fill the gap.
 
  @param q a pointer to an instance of the priqueue_t data structure
  @param index position of element to be removed
  @return the element removed from the queue
  @return NULL if the specified index does not exist
 */
void *priqueue_remove_at(priqueue_t *q, int index)
{
  //index bigger/smaller than queue
  if (index > priqueue_size(q) || index < 0)
  {
    return NULL;
  }
  else
  {
    //traverse until index-1
    node_t *cNode = q->head;

    //remove at head
    if (index == 0)
    {
      node_t *next = cNode->next; //second node
      void *data = cNode->data;
      free(cNode);
      q->head = next;
      q->size--;
      return data;
    }


    int i = 0;
    while (i < index - 1)
    {
      cNode = cNode->next;
      i++;
    }
    //remove next and fix link
    node_t *removeNode = cNode->next;
    void *data = removeNode->data;
    cNode->next = removeNode->next;
    free(removeNode);
    q->size--;
    return data;
  }
}

/**
  Returns the number of elements in the queue.
 
  @param q a pointer to an instance of the priqueue_t data structure
  @return the number of elements in the queue
 */
int priqueue_size(priqueue_t *q)
{
  return q->size;
}

/**
  Destroys and frees all the memory associated with q.
  
  @param q a pointer to an instance of the priqueue_t data structure
 */
void priqueue_destroy(priqueue_t *q)
{
  while (priqueue_size(q) > 0)
  {
    priqueue_poll(q);
  }
}
