#include <string.h>
#include <stdlib.h>
#include "coroutine_impl.h"
#include "epoll.h"
#include "misc.h"

int do_epoll_wait(int epfd, epoll_result_t *result,int maxevents,int timeout) {
  return epoll_wait(epfd,result->events,maxevents,timeout);
}

int do_epoll_ctl(int epfd,int op,int fd,struct epoll_event *ev) {
  return epoll_ctl(epfd,op,fd,ev);
}

int do_epoll_create(int size) {
  return epoll_create(size);
}

static epoll_result_t* alloc_epoll_result(int n) {
  epoll_result_t* ret = (epoll_result_t*)malloc(sizeof(epoll_result_t));

  ret->size = n;
  ret->events = (struct epoll_event*)calloc(1,n * sizeof(struct epoll_event));

  return ret;
}

static void free_epoll_result(epoll_result_t *ret) {
  if(!ret) {
    return;
  }

  if(ret->events) {
    free(ret->events);
  }

  free(ret);
}

static epoll_timer_t* alloc_timer(int size) {
  epoll_timer_t *timer = (epoll_timer_t*)calloc(1, sizeof(epoll_timer_t));
  timer->size = size;

  timer->items = (timer_list_t*)calloc(size, sizeof(timer_list_t));
  timer->start_idx = 0;
  timer->start = GetTickMS();

  return timer;
}

void free_timer(epoll_timer_t *timer) {
  free(timer->items);
  free(timer);
}

epoll_context_t* alloc_epoll(int size) {
  epoll_context_t *ep = (epoll_context_t*)calloc(1, sizeof(epoll_context_t));

  ep->fd = do_epoll_create(size);
  ep->timer = alloc_timer(60000);
  ep->active_list = (timer_list_t*)calloc(1, sizeof(timer_list_t));
  ep->timeout_list = (timer_list_t*)calloc(1, sizeof(timer_list_t));
  ep->result = NULL;
  ep->size = size;

  return ep;
}

void free_epoll(epoll_context_t *epoll) {
  if (epoll != NULL) {
    free(epoll->active_list);
    free(epoll->timeout_list);
    free_epoll_result(epoll->result);
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

static inline void takeAllTimeout(epoll_timer_t *timer, unsigned long long now, timer_list_t *result) {
  if (timer->start == 0) {
    timer->start = now;
    timer->start_idx = 0;
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
    int idx = (timer->start_idx + i) % timer->size;
    join(result, timer->items + idx);
  }

  timer->start = now;
  timer->start_idx = cnt - 1;
}

void addTail(timer_list_t *list, timer_item_t *item) {
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

void removeFromLink(timer_item_t *item) {
  timer_list_t *list = item->parent;
  if (!list) {
    return;
  }

  if (item == list->head) {
    list->head = item->next;
    if (list->head) {
      list->head->prev = NULL;
    }
  } else {
    if (item->prev) {
      item->prev->next = item->next;
    }
  }

  if (item == list->tail) {
    list->tail = item->prev;
    if (list->tail) {
      list->tail->next = NULL;
    }
  } else {
    item->next->prev = item->prev;
  }

  item->prev = item->next = NULL;
  item->parent = NULL;
}

void eventloop() {
  env_t *env = get_curr_thread_env();
  if (env == NULL) {
    init_curr_thread_env();
    env = get_curr_thread_env();
  }

  epoll_context_t *epoll = env->epoll;

  if (epoll->result == NULL) {
    epoll->result = alloc_epoll_result(epoll->size);
  }
  epoll_result_t *result = epoll->result;

  while (1) {
    int i;
    int ret = do_epoll_wait(epoll->fd, result, epoll->size, 1);

    timer_list_t *active = epoll->active_list;
    timer_list_t *timeout = epoll->timeout_list;

    memset(timeout, 0, sizeof(timer_list_t));
    for (i = 0; i < ret; i++) {
      timer_item_t *item = (timer_item_t*)result->events[i].data.ptr;
      if (item->prepare) {
        item->prepare(item, &result->events[i], active);
      } else {
        addTail(active, item);
      }
    }
 
    unsigned long long now = GetTickMS();
    takeAllTimeout(epoll->timer, now, timeout);

    timer_item_t *item = timeout->head;
    while (item != NULL) {
      item->timeout = 1;
      item = item->next;
    }

    join(active, timeout);

    item = active->head;
    while (item != NULL) {
      popHead(active);
      if (item->process != NULL) {
        item->process(item);
      }

      item = active->head;
    }
  }
}

int addTimeout(epoll_timer_t *timer,timer_item_t *item, unsigned long long now) {
	if(timer->start == 0) {
		timer->start = now;
		timer->start_idx = 0;
	}
	if(now < timer->start) {
		return -1;
	}

	if(item->expire < now) {
    return -1;
	}
	int diff = item->expire - timer->start;

	if(diff >= timer->size) {
    return -1;
	}

	addTail(timer->items + (timer->start_idx + diff) % timer->size, item);

	return 0;
}
