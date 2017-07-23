#ifndef __EPOLL_H__
#define __EPOLL_H__

#include <sys/epoll.h>
#include "typedef.h"

typedef void (*prepare_fun_t)(timer_item_t *,struct epoll_event &ev, timer_list_t *active);
typedef void (*process_fun_t)(timer_item_t *);

struct timer_item_t {
  timer_item_t *prev;
  timer_item_t *next;
  timer_list_t *parent;  

  unsigned long long expire;
  prepare_fun_t prepare;  
  process_fun_t process;  

  void *arg;
  bool timeout;
};

struct timer_list_t {
  timer_item_t *head;
  timer_item_t *tail;
};

struct timer_t {
  int size;
  time_list_t items;

  unsigned long long start;
  long long startIdx;
};

struct epoll_result_t {
  int size;
  struct epoll_event *events;
};

struct epoll_t {
  int fd;
  int size;

  timer_t *timer;
  time_list_t *timeout_list;
  time_list_t *active_list;

  epoll_result_t *result;
};

epoll_t* epoll_create(int size);
void eventloop(epoll_t *);

#endif  // __EPOLL_H__
