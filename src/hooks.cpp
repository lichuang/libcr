#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <pthread.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include "scheduler.h"

Scheduler gSched;

struct Init {
  Init();
};

Init gInit;

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

#undef accept
int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
  printf("my accept: %d\n", gSched.get_current());
  return gSched.Accept(sockfd, addr, addrlen);
}

Init::Init() {
  printf("in init\n");
  //accept = my_accept;
}


#undef recv
ssize_t recv(int fd, void *buf, size_t len, int flags) {
  return gSched.Recv(fd, buf, len, flags);
}

#undef send
ssize_t send(int fd, const void *buf, size_t len, int flags) {
  return gSched.Send(fd, buf, len, flags);
}
