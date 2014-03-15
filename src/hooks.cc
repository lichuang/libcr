#include <stdio.h>
#include <unistd.h>
#include "coroutine.h"

extern Scheduler gSched;

extern "C" {
#undef pthread_create
int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void *(*start_routine) (void *), void *arg) {
  *thread = gSched.Spawn(arg, start_routine);

  return 0;
}

#undef accept
int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
  return gSched.Accept(sockfd, addr, addrlen);
}

#undef recv
ssize_t recv(int fd, void *buf, size_t len, int flags) {
  return gSched.Recv(fd, buf, len, flags);
}

#undef send
ssize_t send(int fd, const void *buf, size_t len, int flags) {
  return gSched.Send(fd, buf, len, flags);
}

#undef close
int close(int fd) {
  return gSched.Close(fd);
}
}
