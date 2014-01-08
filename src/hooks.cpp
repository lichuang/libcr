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

#undef listen
unsigned int listen(int fd) {
  return gSched.Listen(fd);
}

#undef recv
ssize_t recv(int fd, void *buf, size_t len, int flags) {
  return gSched.Recv(fd, buf, len, flags);
}

#undef send
ssize_t send(int fd, const void *buf, size_t len, int flags) {
  return gSched.Send(fd, buf, len, flags);
}
