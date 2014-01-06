#ifndef __SCHEDULER_H__
#define __SCHEDULER_H__

#include <list>
#include <ucontext.h>
#include "common.h"

using namespace std;

class Coroutine;

void* ScheduleMain(void *);

class Scheduler {
  friend void* ScheduleMain(void *);
public:
  Scheduler();
  ~Scheduler();

  int  Spawn(cfunc func, void *arg);
  int  Yield(int id);
  int  Resume(int id);

  ucontext_t *get_context();

private:
  int NewCoroutineId();

private:
  Coroutine*          main_;
  int                 epoll_fd_;
  Coroutine**         coros_;
  int                 capacity_;
  int                 num_;
  int                 current_;
  int                 last_;
  list<Coroutine*>    active_;
  list<Coroutine*>    sleep_;
};

#endif  /* __SCHEDULER_H__ */
