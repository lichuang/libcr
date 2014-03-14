#ifndef __COROUTINE_H__
#define __COROUTINE_H__

#include <stdint.h>
#include <ucontext.h>
#include <list>
#include <vector>

using namespace std;

typedef void* (*cfunc)(void*);

struct Coroutine;
void mainfunc(void *ptr);

class Scheduler {
  friend void mainfunc(void *ptr);
public:
  Scheduler();
  ~Scheduler();

  int   Spawn(void *arg, cfunc fun);
  void  Yield();
  void  Resume(int id);
  int   Status(int id);

  void  Run();
private:
  int   NewId();

private:
  int                 running_;
  size_t              num_;
  list<Coroutine*>    active_;
  list<Coroutine*>    suspend_;
  vector<Coroutine*>  coros_;
  ucontext_t          main_;
};

extern Scheduler gSched;

#endif  /* __COROUTINE_H__ */
