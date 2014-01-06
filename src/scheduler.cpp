#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "coroutine.h"
#include "scheduler.h"

void* ScheduleMain(void *);

Scheduler::Scheduler()
  : epoll_fd_(-1),
    coros_(NULL),
    capacity_(kCoroNum),
    num_(0),
    current_(-1),
    last_(0) {
  coros_ = (Coroutine**)malloc(sizeof(Coroutine*) * capacity_);
  memset(coros_, 0, sizeof(Coroutine*) * capacity_);

  current_ = NewCoroutineId();
  main_ = new Coroutine(this, current_, ScheduleMain, this);
  coros_[current_] = main_;
}

Scheduler::~Scheduler() {
  free(coros_);
}

int
Scheduler::NewCoroutineId() {
  if (num_ >= capacity_) {
    capacity_ *= 2;
    coros_ = (Coroutine**)realloc(coros_, capacity_);
    if (coros_ == NULL) {
      return -1;
    }
  }

  int id, i;
  for (i = 0; i < capacity_; ++i) {
    id = (i + last_) % capacity_;
    if (coros_[id] == NULL) {
      last_ = id;
      ++num_;
      return id;
    }
  }

  // TODO:check here
  return -1;
}

ucontext_t *
Scheduler::get_context() {
  return main_->get_context();
}

int
Scheduler::Spawn(cfunc func, void *arg) {
  int id;

  id = NewCoroutineId();
  if (id == -1) {
    return -1;
  }

  Coroutine *coro = new Coroutine(this, id, func, arg);
  if (!coro) {
    return -1;
  }
  coros_[id] = coro;

  active_.push_back(coro);
  ucontext_t *context = coro->get_context();
  current_ = id;
  swapcontext(main_->get_context(), context);

  return id;
}

int
Scheduler::Yield(int id) {
  return 0;
}

int
Scheduler::Resume(int id) {
  return 0;
}

void*
ScheduleMain(void *arg) {
  Scheduler *sched = (Scheduler*)arg;
  list<Coroutine*>::iterator iter;
  Coroutine *coro;
  ucontext_t *main;
  int id;

  main = sched->get_context();
  while (true) {
    while (sched->active_.empty()) {
      usleep(100);
    }
    for (iter = sched->active_.begin();
         iter != sched->active_.end(); ++iter) {
      coro = *iter;
      id = coro->get_id();

      swapcontext(main,coro->get_context()); 
      int status = coro->get_status();
      if (status == DEAD) {
        delete coro;
        sched->active_.erase(iter);
        sched->coros_[id] = NULL;
        --sched->num_;
      } else if (status == SUSPEND) {
        sched->active_.erase(iter);
        //sleep_.push_back(coros_);
      }
    }
  }

  return NULL;
}
