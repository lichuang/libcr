#ifndef __COROUTINE_H__
#define __COROUTINE_H__

#include <ucontext.h>
#include "common.h"

class Scheduler;

enum CStatus {
  DEAD    = 0,
  READY   = 1,
  RUNNING = 2,
  SUSPEND = 3,
};

void* CoroMain(void *arg);

class Coroutine {
  friend void* CoroMain(void *arg);
public:
  Coroutine(Scheduler *sched, int id, cfunc func, void *arg);

  ~Coroutine();

  int get_status() const {
    return status_;
  }

  void set_status(int status) {
    status_ = status;
  }

  int get_id() const {
    return id_;
  }

  ucontext_t* get_context() {
    return &context_;
  }

  int Yield();
  int Resume();

private:
  Scheduler  *sched_;
  int         id_;
  ucontext_t  context_;
  char       *stack_;
  int         stack_size_;
  int         status_;
  cfunc       func_;
  void       *arg_;
  int         parent_;
};

#endif  /* __COROUTINE_H__ */
