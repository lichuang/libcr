#ifndef __COROUTINE_H__
#define __COROUTINE_H__

#include <stdint.h>
#include <ucontext.h>
#include <arpa/inet.h> //inet_addr
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/socket.h>
#include <list>
#include <vector>
#include "idmap.h"

using namespace std;

typedef void* (*cfunc)(void*);

struct Coroutine;
struct Socket;
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

  int   Accept(int sockfd, struct sockaddr *addr,
               socklen_t *addrlen);
  ssize_t    Recv(int fd, void *buf, size_t len, int flags);
  ssize_t    Send(int fd, const void *buf, size_t len, int flags);
  int  Close(int fd);
private:
  void  CheckNetwork();

private:
  int                 epfd_;
  int                 running_;
  size_t              num_;
  list<Coroutine*>    active_;
  vector<Coroutine*>  coros_;
  vector<Socket*>     socks_;
  ucontext_t          main_;
  IdMap               id_map_;
};

extern Scheduler gSched;

#endif  /* __COROUTINE_H__ */
