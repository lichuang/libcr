#include <stdio.h>
#include "coroutine.h"
#include "scheduler.h"

Coroutine::Coroutine(Scheduler *sched, int id, cfunc func, void *arg)
  : sched_(sched),
    id_(id),
    stack_(NULL),
    stack_size_(kDefaulaStackSize),
    status_(READY),
    func_(func),
    arg_(arg),
    parent_(-1) {
  stack_ = new char[stack_size_ + kProtectStackSize];

  getcontext(&context_);
  ucontext_t *main = sched->get_context();
  context_.uc_stack.ss_sp = stack_;
  context_.uc_stack.ss_size = stack_size_;
  context_.uc_link = main;

  makecontext(&context_, (void (*)())CoroMain, 1, this);
}

Coroutine::~Coroutine() {
  delete [] stack_;
}

void*
CoroMain(void *arg) {
  Coroutine *coro = (Coroutine*)arg;
  void *ret = coro->func_(coro->arg_);
  coro->status_ = DEAD;
  return ret;
}
