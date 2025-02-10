/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * Copyright (C) 2018 Institute of Computing
 * Technology, CAS Author : Han Shukai (email :
 * hanshukai@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * Changelog: 2019-8 Reimplement queue.h.
 * Provide Linux-style doube-linked list instead of original
 * unextendable Queue implementation. Luming
 * Wang(wangluming@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * */

#ifndef INCLUDE_LIST_H_
#define INCLUDE_LIST_H_

#include <assert.h>
#include <type.h>
// double-linked list
typedef struct list_node {
  struct list_node *next, *prev;
} list_node_t;

typedef list_node_t list_head;

// LIST_HEAD is used to define the head of a list.
#define LIST_HEAD(name) struct list_node name = {&(name), &(name)}

/* TODO: [p2-task1] implement your own list API */
static inline int list_is_empty(const list_head *queue) {
  return queue->next == queue;
}

static inline void list_push_front(list_head *head, list_node_t *node) {
  node->next = head->next;
  node->prev = head;
  head->next->prev = node;
  head->next = node;
}

static inline void list_push_back(list_head *head, list_node_t *node) {
  node->prev = head->prev;
  node->next = head;
  head->prev->next = node;
  head->prev = node;
}

static inline void list_remove(list_node_t *node) {
  node->prev->next = node->next;
  node->next->prev = node->prev;
}

static inline list_node_t *list_front(const list_head *head) {
  return list_is_empty((list_head *)head) ? NULL : head->next;
}

static inline void enqueue(list_head *queue, list_node_t *node) {
  // if (node != queue->next) {
  //   list_push_back(queue, node);
  // } else {
  //   assert(0);
  // }
  int has = 0;
  for (list_node_t *tmp = queue->next; tmp != queue; tmp = tmp->next) {
    if (tmp == node) {
      has = 1;
    }
  }
  if (!has) {
    list_push_back(queue, node);
  }
}

static inline list_node_t *dequeue(list_head *queue) {
  if (list_is_empty(queue)) {
    return NULL;
  }
  list_node_t *front = list_front(queue);
  list_remove(front);
  return front;
}

static inline int is_queue_empty(const list_head *queue) {
  return list_is_empty(queue);
}

static inline int queue_len(const list_head *queue) {
  int len = 0;
  list_node_t *node = queue->next;
  while (node != queue) {
    len++;
    node = node->next;
  }
  return len;
}

static inline list_node_t *get_i_th_node(int i, const list_head *queue) {
  list_node_t *node = queue->next;
  int no = 0;
  while (no != i) {
    no++;
    node = node->next;
  }
  return node;
}

#endif
