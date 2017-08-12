#ifndef __COROUTINE_TASK_H__
#define __COROUTINE_TASK_H__

#include "coroutine.h"
#include "typedef.h"

typedef enum task_state_t {
  TASK_RUNNING,
  TASK_WAITING,
  TASK_END,
} task_state_t;

struct task_t {
  coroutine_t *coroutine;
  void *arg;
  task_state_t state;

  taskpool_t *pool;

  // for free list
  task_t *next;

  coroutine_task_attr_t attr;
  char timeout;

  int leftmsec;
  unsigned long long start;
  unsigned long long last;
};

struct taskpool_t {
  task_t **tasks;

  task_t *free;
  env_t *env;
  int size;
  int working;
};

taskpool_t* create_thread_taskpool(env_t *env, int size);
int new_task(taskpool_t *, coroutine_task_attr_t *attr);

#endif  // __COROUTINE_TASK_H__
