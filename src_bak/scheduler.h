#ifndef __SCHEDULER_H__
#define __SCHEDULER_H__

#include <list>
#include <map>
#include <ucontext.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include "common.h"

using namespace std;

class Coroutine;

void* ScheduleMain(void *);

class Scheduler {
  friend void* ScheduleMain(void *);
public:
  Scheduler();
  ~Scheduler();

  int  Spawn(cfunc func, void *arg);
  void Run();

  ucontext_t *get_context();

  unsigned int Sleep(unsigned int seconds);
  int          Accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
  ssize_t      Recv(int fd, void *buf, size_t len, int flags);
  ssize_t      Send(int fd, const void *buf, size_t len, int flags);

  int          get_current() const {
    return current_;
  }
private:
  int NewCoroutineId();
  void CheckNetwork();

private:
  struct Socket {
    int               fd;
    Coroutine        *co;
  };

  ucontext_t          main_;
  //Coroutine*          main_;
  int                 epoll_fd_;
  Coroutine**         coros_;
  int                 capacity_;
  int                 num_;
  int                 current_;
  int                 last_;
  list<Coroutine*>    active_;
  multimap<unsigned int, Coroutine*> sleep_;
  map<int, Socket*>   socks_;
};

#endif  /* __SCHEDULER_H__ */
