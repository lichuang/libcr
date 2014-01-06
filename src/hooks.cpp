#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <pthread.h>
#include "scheduler.h"

Scheduler gSched;

#undef pthread_create
int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void *(*start_routine) (void *), void *arg) {
  printf("in pthread_create\n");
  return gSched.Spawn(start_routine, arg);
}
