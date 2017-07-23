#ifndef __COROUTINE_IMPL_H__
#define __COROUTINE_IMPL_H__

#include "coroutine.h"
#include "typedef.h"

static const kMaxCallStack = 128;

struct env_t {
  coroutine_t *callstack[kMaxCallStack];
  int callstacksize;
  bool main_running;
  coroutine_t *main;
  epoll_t *epoll;
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
  coroutine_env_t *env;
  coroutine_context_t context;
  void *arg;
  bool isMain;
  bool isEnableSysHook;

  state_t state;

  context_t context;

  stack_t *stack;
};

#endif  // __COROUTINE_IMPL_H__
