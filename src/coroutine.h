#ifndef __COROUTINE_H__
#define __COROUTINE_H__

struct coroutine_t;
typedef coroutine_t coroutine_t;

typedef int (*coroutine_fun_t)(void *);

typedef struct coroutine_attr_t {
  int stack_size;
} coroutine_attr_t;

coroutine_t* coroutine_new(coroutine_attr_t *, coroutine_fun_t fun, void *arg);
coroutine_t* coroutine_main(coroutine_attr_t *, coroutine_fun_t fun, void *arg);
void coroutine_resume(coroutine_t *);
void coroutine_yield(coroutine_t *);
void coroutine_free(coroutine_t *)

#endif  // __COROUTINE_H__
