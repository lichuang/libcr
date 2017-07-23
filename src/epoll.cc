#include <string.h>
#include "epoll.h"
#include "misc.h"

int co_epoll_wait(int epfd, epoll_result_t *result,int maxevents,int timeout) {
  return epoll_wait(epfd,result->events,maxevents,timeout);
}

int co_epoll_ctl(int epfd,int op,int fd,struct epoll_event *ev) {
  return epoll_ctl(epfd,op,fd,ev);
}

int co_epoll_create(int size) {
  return epoll_create(size);
}

epoll_result_t* co_epoll_res_alloc(int n) {
  epoll_result_t* ret = (epoll_result_t*)malloc(sizeof(epoll_result_t));

  ret->size = n;
  ret->events = (struct epoll_event*)calloc(1,n * sizeof(struct epoll_event));

  return ret;
}

void co_epoll_res_free(epoll_result_t *ret) {
  if(!ret) {
    return;
  }

  if(ret->events) {
    free(ret->events);
  }

  free(ret);
}

static timer_t* alloc_timer(int size) {
  timer_t *timer = (timer_t*)calloc(1, sizeof(timer_t));
  timer->size = size;

  timer->items = (time_list_t*)calloc(size, sizeof(timer_list_t));
  timer->startIdx = 0;
  timer->start = GetTickMS();

  return timer;
}

void free_timer(timer_t *timer) {
  free(timer->items);
  free(timer);
}

epoll_t* alloc_epoll(int size) {
  epoll_t *ep = (epoll_t*)calloc(1, sizeof(epoll_t));

  ep->fd = co_epoll_create(size);
  ep->timer = alloc_timer(60000);
  ep->active_list = (timer_list_t*)calloc(1, sizeof(timer_list_t));
  ep->timeout_list = (timer_list_t*)calloc(1, sizeof(timer_list_t));
  ep->result = NULL;
  ep->size = size;

  return ep;
}

void free_epoll(epoll_t *epoll) {
  if (epoll != NULL) {
    free(epoll->active_list);
    free(epoll->timeout_list);
    co_epoll_res_free(epoll->result);
    free_timer(epoll->timer);

    free(epoll);
  }
}

static inline void popHead(timer_list_t *list) {
  if (!list->head) {
    return;
  }

  timer_item_t *item = list->head;
  if (list->head == list->tail) {
    list->head = list->tail = NULL;
  } else {
    list->head = list->head->next;
  }

  item->prev = item->next = NULL;
  item->parent = NULL;

  if (list->head != NULL) {
    list->head->prev = NULL;
  }
}

static inline void join(timer_list_t *list, timer_list_t *other) {
  if (!other->head) {
    return;
  }

  timer_item_t *item = other->head;
  while (item != NULL) {
    item->parent = list;
    item = item->next;
  }

  item = other->head;
  if (list->tail != NULL) {
    list->tail->next = item;
    item->prev = list->tail;
    list->tail = other->tail;
  } else {
    list->head = other->head;
    list->tail = other->tail;
  }

  other->head = other->tail = NULL;
}

static inline void takeAllTimeout(timer_t *timer, unsigned long long now, timer_list_t *result) {
  if (timer->start == 0) {
    timer->start = now;
    timer->startIdx = 0;
  }

  if (now < timer->start) {
    return;
  }

  int cnt = now - timer->start + 1;
  if (cnt > timer->size) {
    cnt = timer->size;
  }
  if (cnt < 0) {
    return;
  }

  int i;
  for (i = 0; i < cnt; i++) {
    int idx = (timer->startIdx + i) % timer->size;
    join(result, timer->items + idx);
  }

  timer->start = now;
  timer->startIdx = cnt - 1;
}

static inline void addTail(timer_list_t *list, timer_item_t *item) {
  if (item->parent != NULL) {
    return;
  }

  if (list->tail != NULL) {
    list->tail->next = item;
    item->next = NULL;
    item->prev = list->tail;
    list->tail = item;
  } else {
    list->head = list->tail = item;
    item->next = item->prev = NULL;
  }

  item->parent = list;
}

void eventloop(epoll_t *epoll) {
  if (epoll->result == NULL) {
    epoll->result = co_epoll_res_alloc(ep->size);
  }
  epoll_result_t *result = result;

  while (true) {
    int i;
    int ret = co_epoll_wait(ep->fd, result, ep->size, 1);

    timer_list_t *active = ep->active_list;
    timer_list_t *timeout = ep->timeout_list;

    memset(timeout, 0, sizeof(timer_list_t));
    for (i = 0; i < ret; i++) {
      timer_item_t *item = (timer_item_t*)result->events[i].data.ptr;
      if (item->prepare) {
        item->prepare(item, result->events[i], active);
      } else {
        addTail(active, item);
      }
    }
 
    unsigned long long now = GetTickMS();
    takeAllTimeout(epoll->timer, now, timeout);

    timer_item_t *item = timeout->head;
    while (item != NULL) {
      item->timeout = true;
      item = item->next;
    }

    join(active, timeout;);

    item = active->head;
    while (item != NULL) {
      popHead(active);
      if (item->process != NULL) {
        item->process(process);
      }

      item = active->head;
    }
  }
}
