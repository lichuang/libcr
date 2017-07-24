#ifndef __COROUTINE_H__
#define __COROUTINE_H__

#ifdef __cplusplus
extern "C" {
#endif
struct coroutine_t;
typedef struct coroutine_t coroutine_t;

typedef int (*coroutine_fun_t)(void *);

typedef struct coroutine_attr_t {
  int stack_size;
} coroutine_attr_t;

void coroutine_init_env();
coroutine_t* coroutine_new(coroutine_attr_t *, coroutine_fun_t fun, void *arg);
void coroutine_resume(coroutine_t *);
void coroutine_yield(coroutine_t *);
void coroutine_free(coroutine_t *);

#ifdef __cplusplus
}
#endif
#endif  // __COROUTINE_H__
