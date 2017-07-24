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
static const int kMaxCallStack = 128;

struct env_t {
  coroutine_t *callstack[kMaxCallStack];
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

enum state_t {
  INIT,
  RUNNING,
  STOPPED
};

struct coroutine_t {
  coroutine_fun_t fun;
  env_t *env;
  void *arg;
  bool main;
  bool enable_sys_hook;

  state_t state;

  context_t context;

  char *stack_sp;
  unsigned int save_size;
  char *save_buffer;

  stack_t *stack;
};

typedef int (*poll_fun_t)(struct pollfd fds[], nfds_t nfds, int timeout);
int poll_inner(epoll_context_t *ctx, struct pollfd fds[], nfds_t nfds, int timeout, poll_fun_t pollfunc);

env_t* get_curr_thread_env();
void init_curr_thread_env();

#ifdef __cplusplus
}
#endif

#endif  // __COROUTINE_IMPL_H__
