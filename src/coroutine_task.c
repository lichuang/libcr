#include <stdlib.h>
#include <string.h>
#include "assert.h"
#include "coroutine_impl.h"
#include "coroutine_task.h"

static void *task_main(void *arg) {
  task_t *task = (task_t*)arg;

  while (1) {
    if (task->state == TASK_WAITING) {
      coroutine_yield_context();
      continue;
    }

    if (task->state == TASK_END) {
      break;
    }

    task->pool->working++;
    task->coroutine->fun(task->arg); 

    if (task->timeout && task->attr.timeout) {
      task->attr.timeout(task->coroutine->arg);
    }

    // return to free list
    task->next = task->pool->free;
    task->pool->free = task;
    task->pool->working--;

    // reset param
    task->arg = NULL;
    task->coroutine->fun = NULL;
    task->state = TASK_WAITING;
  }

  return NULL;
}

int new_task(taskpool_t *pool, coroutine_task_attr_t *attr) {
  if (pool->working == pool->size) {
    return -1;
  }

  task_t *task = pool->free;
  if (task == NULL) {
    return -1;
  }

  pool->free = task->next;
  task->next = NULL;

  task->arg = attr->arg;
  task->coroutine->fun = attr->fun;
  task->state = TASK_RUNNING;
  task->leftmsec = attr->max_timeout_ms;
  task->timeout = 0;
  task->last = get_now();
  memcpy(&task->attr, attr, sizeof(coroutine_task_attr_t));
  coroutine_resume(task->coroutine);

  return 0;
}

taskpool_t* create_thread_taskpool(env_t *env, int size) {
  taskpool_t *pool = (taskpool_t*)malloc(sizeof(taskpool_t));
  memset(pool, 0, sizeof(taskpool_t));
  pool->tasks = (task_t**)malloc(sizeof(task_t*));

  for (int i = 0;i < size; ++i) {
    task_t *task = (task_t*)malloc(sizeof(task_t));
    memset(task, 0, sizeof(task_t));
    pool->tasks[i] = task;
    task->pool = pool;
    coroutine_t *co = coroutine_new(task_main, task);
    co->task = task;
    task->coroutine = co;
    task->next = pool->free;
    task->state = TASK_WAITING;
    pool->free = task;
    coroutine_resume(co);
  }
  pool->env = env;
  pool->size = size;

  return pool;
}
