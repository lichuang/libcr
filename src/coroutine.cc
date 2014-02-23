//#include <assert.h>
#include "coroutine.h"

// stack size
static const int kProtectStackSize = 10 * 1024;
static const int kDefaulaStackSize = 100 * 1024;

// coroutine num
static const int kCoroNum = 256;

static const int kStatusDead    = 0;
static const int kStatusReady   = 1;
static const int kStatusRunning = 2;
static const int kStatusSuspend = 3;

struct Coroutine {
  void       *arg_;
  cfunc       fun_;
  int         id_;
  char       *stack_;
  int         status_;
  ucontext_t  ctx_;

  Coroutine(void *arg, cfunc fun, int id)
    : arg_(arg),
      fun_(fun),
      id_(id),
      stack_(NULL),
      status_(kStatusReady) {
    stack_ = new char[kProtectStackSize + kDefaulaStackSize];
  }

  ~Coroutine() {
    delete [] stack_;
  }
};

Scheduler::Scheduler()
  : running_(-1),
    num_(0) {
  coros_.resize(kCoroNum, NULL);
}

Scheduler::~Scheduler() {
}

int Scheduler::NewId() {
  if (num_ == coros_.size()) {
    coros_.resize(coros_.size() + kCoroNum);
    return num_;
  }
  size_t i;
  for (i = 0; i < coros_.size(); ++i) {
    if (coros_[i] == NULL) {
      break;
    }
  }
  return i;
}

int Scheduler::Spawn(void *arg, cfunc fun) {
  int id;
  Coroutine *coro;

  id = NewId();
  coro = new Coroutine(arg, fun, id);

  return id;
}

void Scheduler::Yield() {
  int id;
  Coroutine *coro;

  id = running_;
  coro = coros_[id];
  coro->status_ = kStatusSuspend;
  running_ = -1;
  swapcontext(&coro->ctx_, &main_);
}

void
mainfunc(void *ptr) {
  Scheduler *sched = (Scheduler *)ptr;
  int id = sched->running_;
  Coroutine *coro = sched->coros_[id];
  coro->fun_(coro->arg_);
/*
  _co_delete(C);
  S->co[id] = NULL;
  --S->nco;
  S->running = -1;
*/
} 

void Scheduler::Resume(int id) {
  Coroutine *coro = coros_[id];

  if (coro == NULL) {
    return;
  }
  int status = coro->status_;
  switch(status) { 
  case kStatusReady:
    getcontext(&coro->ctx_);
    //coro->ctx.uc_stack.ss_sp = S->stack;
    //C->ctx.uc_stack.ss_size = STACK_SIZE;
    coro->ctx_.uc_link = &main_;
    running_ = id;
    coro->status_ = kStatusRunning;
    makecontext(&coro->ctx_, (void (*)(void))mainfunc, 1, this);
    swapcontext(&main_, &coro->ctx_);
    break;
  case kStatusSuspend:
    running_ = id;
    coro->status_ = kStatusRunning;
    swapcontext(&main_, &coro->ctx_);
    break;
  default:
    //assert(0);
    break;
  }
}
