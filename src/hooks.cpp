#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <pthread.h>
#include "scheduler.h"

Scheduler gSched;

void
Run() {
  gSched.Run();
}

#undef pthread_create
int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void *(*start_routine) (void *), void *arg) {
  return gSched.Spawn(start_routine, arg);
}

#undef sleep
unsigned int sleep(unsigned int seconds) {
  return gSched.Sleep(seconds);
}
