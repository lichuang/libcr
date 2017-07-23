#include <pthread.h>
#include "assert.h"
#include "coroutine.h"
#include "coroutine_impl.h"
#include "context.h"

static const kMinStackSize = 128 * 1024;
static const kMaxStackSize = 8 * 1024 * 1024;

static env_* gEnv[204800] = { NULL };

static inline pid_t get_pid() {
  char **p = (char**)pthread_self();
  return p ? *(pid_t*)(p + 18) : getpid();
}

static coroutine_env_t* get_curr_thread_env() {
  return gEnv[get_pid()];
}

static stack_t* alloc_stack(int size) {
  stack_t *stack = (stack_t*)malloc(sizeof(stack_t));
  stack->size = size;
  stack->start = (char*)malloc(sizeof(char) * size);
  stack->end = stack->start + size;

  return stack;
}

static coroutine_t* create_env(env_t *env, coroutine_attr_t *attr, co_fun_t *func, void *arg) {
  int stack_size = attr ? attr->stack_size : kMinStackSize;

  if (stack_size < kMinStackSize) {
    stack_size = kMinStackSize;
  } else if (stack_size > kMaxStackSize) {
    stack_size = kMaxStackSize;
  }

  if (stack_size & 0XFFF) {
    stack_size &= ~0XFFF;
    stack_size += 0x1000;
  }

  coroutine_t *co = (coroutine_t*)malloc(sizeof(coroutine_t));
  co->env = env;
  co->fun = fun;
  co->arg = arg;

  stack_t *stack = alloc_stack(stack_size);
  co->stack = stack;

  return co;
}

static init_curr_thread_env() {
  pid_t pid = 0;
  coroutine_env_t *env = NULL;

  pid = get_pid();
  env = (coroutine_env_t*)calloc(1, sizeof(coroutine_env_t));
  gEnv[pid] = env;

  env->isMain = true;
  coroutine_t *co = create_env(env, NULL, NULL, NULL);

  context_init(&co->context);
  env->callstack[env->callstacksize++] = co;
  env->epoll = alloc_epoll(10240);
}

coroutine_t* do_create_coroutine(coroutine_attr_t *, coroutine_fun_t fun, void *arg, bool main) {
  env_t *env = get_curr_thread_env();
  if (main) {
    ASSSERT(env == NULL);
  } else {
    ASSSERT(env != NULL);
  }

  if (env == NULL) {
    init_curr_thread_env();
    env = get_curr_thread_env();
  }

  if (!main) {
    coroutine_t *co = create_env(co, attr, fun, arg);
  }

  return co;
}

coroutine_t* coroutine_new(coroutine_attr_t *attr, coroutine_fun_t fun, void *arg) {
  return do_create_coroutine(attr, fun, arg, false);
}

void coroutine_free(coroutine_t *co) {
  free(co);
}

static void yield_env(env_t *env) {
  int size = env->callstacksize;
  coroutine_t *last = env->callstack[size - 2];
  coroutine_t *curr = env->callstack[size - 1];
  env->callstacksize--;

  context_swap(curr, last);
}

static int cotoutine_main(void *arg, void *) {
  coroutine_t *co = (coroutine_t*)arg;
  if (co->fun) {
    co->fun(co->arg);
  }
  co->state = STOPPED

  yield_env(co->env);

  return 0;
}

void coroutine_resume(coroutine_t *co) {
  env_t *env = co->env;
  coroutine_t *curr = env->callstack[env->callstacksize - 1];
  if (co->state != RUNNING) {
    context_make(&co->context, cotoutine_main, co, NULL);
    co->state = RUNNING;
  }

  env->callstack[env->callstacksize++] = co;
  context_swap(curr, co);
}

void coroutine_yield(coroutine_t *co) {
  yield_env(co->env);
}

coroutine_t* coroutine_main(coroutine_attr_t *, coroutine_fun_t fun, void *arg) {
  return do_create_coroutine(attr, fun, arg, true);
}
