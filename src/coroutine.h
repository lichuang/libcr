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

  char enable_sys_hook;
} coroutine_options_t;

void coroutine_init_env(const coroutine_options_t *options);
coroutine_t* coroutine_new(coroutine_fun_t fun, void *arg);
void coroutine_resume(coroutine_t *co);
void coroutine_eventloop();

#ifdef __cplusplus
}
#endif
#endif  // __COROUTINE_H__
