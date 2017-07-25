#ifndef __COROUTINE_IMPL_H__
#define __COROUTINE_IMPL_H__

#include <poll.h>
#include "typedef.h"
#include "coroutine.h"
#include "context.h"
#include "epoll.h"

#ifdef __cplusplus
extern "C" {
#endif

struct env_t {
  coroutine_t **callstack;
  int callstacksize;
  coroutine_t *main;
  epoll_context_t *epoll;

  coroutine_t *occupy;
  coroutine_t *pending;
};

struct stack_t {
  coroutine_t *coroutine;
  int size;
  char *end;
  char *start;
};

typedef enum state_t {
  INIT,
  RUNNING,
  STOPPED
} state_t;

struct coroutine_t {
  coroutine_fun_t fun;
  env_t *env;
  void *arg;
  char main;
  char enable_sys_hook;

  state_t state;

  context_t context;

  char *stack_sp;
  unsigned int save_size;
  char *save_buffer;

  stack_t *stack;
};

typedef int (*poll_fun_t)(struct pollfd fds[], nfds_t nfds, int timeout);
int poll_inner(epoll_context_t *ctx, struct pollfd fds[], nfds_t nfds, int timeout, poll_fun_t pollfunc);

int	coroutine_poll(epoll_context_t *ctx,struct pollfd fds[], nfds_t nfds, int timeout_ms);
epoll_context_t *get_epoll_context();

env_t* get_curr_thread_env();
void init_curr_thread_env();

char is_enable_sys_hook();

#ifdef __cplusplus
}
#endif

#endif  // __COROUTINE_IMPL_H__
