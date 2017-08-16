#ifndef __COROUTINE_H__
#define __COROUTINE_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef void* (*coroutine_fun_t)(void *);

struct coroutine_t;
typedef struct coroutine_t coroutine_t;

typedef struct coroutine_options_t {
  int stack_size;

  int task_per_thread;
} coroutine_options_t;

void coroutine_init_env(const coroutine_options_t *options);

typedef struct coroutine_task_attr_t {
  char enable_sys_hook;

  // -1 means no timeout
  int max_timeout_ms;

  void *arg;

  coroutine_fun_t fun;

  // coroutine timeout callback
  // if max_timeout_ms == -1, then ignored
  coroutine_fun_t timeout;
} coroutine_task_attr_t;

// return 0 success, return -1 fail
int coroutine_new_task(coroutine_task_attr_t *attr);

void* coroutine_arg();

void* coroutine_thread_arg();

void coroutine_init_eventloop(void *arg);
void coroutine_eventloop();

#ifdef __cplusplus
}
#endif
#endif  // __COROUTINE_H__
