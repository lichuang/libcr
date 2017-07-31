#ifndef __EPOLL_H__
#define __EPOLL_H__

#include <sys/epoll.h>
#include "typedef.h"
#include "coroutine.h"

typedef void (*prepare_fun_t)(timer_item_t *,struct epoll_event *ev, timer_list_t *active);
typedef void (*process_fun_t)(timer_item_t *);

struct timer_item_t {
  timer_item_t *prev;
  timer_item_t *next;
  timer_list_t *parent;  

  unsigned long long expire;
  prepare_fun_t prepare;  
  process_fun_t process;  

  coroutine_t *coroutine;
  void *arg;
  char timeout;
};

struct timer_list_t {
  timer_item_t *head;
  timer_item_t *tail;
};

struct epoll_timer_t {
  int size;
  timer_list_t *items;

  unsigned long long start;
  long long start_idx;
};

struct epoll_result_t {
  int size;
  struct epoll_event *events;
};

struct epoll_context_t {
  int fd;
  int size;

  unsigned long long now;

  epoll_timer_t *timer;
  timer_list_t *timeout_list;
  timer_list_t *active_list;

  epoll_result_t *result;
};

epoll_context_t* alloc_epoll(int size);
int do_epoll_wait(int epfd, epoll_result_t *result,int maxevents,int timeout);
int do_epoll_ctl(int epfd,int op,int fd,struct epoll_event *ev);
int do_epoll_create(int size);

int add_timeout(epoll_timer_t *timer,timer_item_t *item, unsigned long long now);
void remove_from_link(timer_item_t *item);
void add_tail(timer_list_t *list, timer_item_t *item);

int do_epoll_ctl(int epfd,int op,int fd,struct epoll_event *ev);
unsigned long long get_epoll_now(epoll_context_t *epoll);

#endif  // __EPOLL_H__
