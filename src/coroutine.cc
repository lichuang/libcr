#include <errno.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>
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

typedef int (*sysAccept)(int, struct sockaddr *, socklen_t *);
typedef ssize_t (*sysRecv)(int fd, void *buf, size_t len, int flags);
typedef ssize_t (*sysSend)(int fd, const void *buf, size_t len, int flags);
typedef int (*sysClose)(int fd);

static sysAccept gSysAccept;
static sysRecv   gSysRecv;
static sysSend   gSysSend;
static sysClose  gSysClose;

Scheduler gSched;

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

struct Socket {
  int         fd_;
  Coroutine*  coro_;  
};

Scheduler::Scheduler()
  : epfd_(-1),
    running_(-1),
    num_(0) {
  coros_.resize(kCoroNum, NULL);
  socks_.resize(kCoroNum, NULL);

  epfd_ = epoll_create(1024);

  gSysAccept = (sysAccept)dlsym(RTLD_NEXT, "accept");
  gSysRecv   = (sysRecv)dlsym(RTLD_NEXT, "recv");
  gSysSend   = (sysSend)dlsym(RTLD_NEXT, "send");
  gSysClose  = (sysClose)dlsym(RTLD_NEXT, "close"); 
}

Scheduler::~Scheduler() {
  size_t i;

  for (i = 0; i < coros_.size(); ++i) {
    if (coros_[i]) {
      delete coros_[i];
    }
    if (socks_[i]) {
      delete socks_[i];
    } 
  }
}

int
Scheduler::Spawn(void *arg, cfunc fun) {
  int id;
  Coroutine *coro;

  id = id_map_.Allocate();
  if (id < 0) {
    return id;
  }
  coro = new Coroutine(arg, fun, id);
  coros_[id] = coro;
  active_.push_back(coro);

  return id;
}

void
Scheduler::Yield() {
  int id;
  Coroutine *coro;

  id = running_;
  coro = coros_[id];
  coro->status_ = kStatusSuspend;
  running_ = -1;
  swapcontext(&coro->ctx_, &main_);
}

int
Scheduler::Status(int id) {
  Coroutine *coro;

  coro = coros_[id];
  if (coro == NULL) {
    return kStatusDead;
  }

  return coro->status_;
}

void
mainfunc(void *) {
  Scheduler *sched = &gSched;
  int id = sched->running_;
  Coroutine *coro = sched->coros_[id];
  coro->fun_(coro->arg_);

  sched->id_map_.Free(coro->id_);
  delete coro;
  sched->coros_[id] = NULL;
  --sched->num_;
  sched->running_ = -1;
} 

void
Scheduler::Resume(int id) {
  Coroutine *coro = coros_[id];

  if (coro == NULL) {
    return;
  }
  int status = coro->status_;
  switch(status) { 
  case kStatusReady:
    getcontext(&coro->ctx_);
    coro->ctx_.uc_stack.ss_sp = coro->stack_;
    coro->ctx_.uc_stack.ss_size = kDefaulaStackSize;
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
    break;
  }
}

void
Scheduler::Run() {
  while (1) {
    list<Coroutine*>::iterator iter;
    for (iter = active_.begin(); iter != active_.end(); ++iter) {
      Resume((*iter)->id_);
    }
    CheckNetwork();
  }
}

int
Scheduler::Accept(int fd, struct sockaddr *addr,
                  socklen_t *addrlen) {
  Coroutine *coro = coros_[running_];

  epoll_event ev;
  memset(&ev, 0, sizeof(struct epoll_event));
  Socket *sock = socks_[fd];
  if (sock == NULL) {
    sock        = new Socket();
    socks_[fd]  = sock;
    sock->fd_   = fd;
    sock->coro_ = coro;
  }
  ev.data.ptr = (void*)sock;
  ev.events = EPOLLIN;

  if (epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &ev) != 0) {
    printf("epoll_ctl error:%s\n", strerror(errno));
    return -1;
  }

swap:  
  coro->status_ = kStatusSuspend;
  swapcontext(&coro->ctx_, &main_);
  epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, &ev);

  int s;
  do { 
    s = gSysAccept(fd, addr, addrlen);
    if (s < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        goto swap;
      } else if (errno != EINTR) {
        printf("accept errno: %s\n", strerror(errno));
        return -1;
      } else {
        // EINTR
        continue;
      }
    }
    fcntl(s, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
    break;
  } while(true);

  return s;
}

void
Scheduler::CheckNetwork() {
  epoll_event events[1024];
  int nfds = epoll_wait(epfd_, events, 1024, -1);

  if (nfds > 0) { 
    int i;
    Socket *sock;

    for(i = 0 ; i < nfds ; ++i) {
      if(events[i].events & (EPOLLIN || EPOLLOUT)) {
        sock = (Socket*)events[i].data.ptr;
        active_.push_back(sock->coro_);
        continue;
      }
    }
  } else {
    printf("epoll error: %s:%d\n", strerror(errno), errno);
  }
}

ssize_t
Scheduler::Recv(int fd, void *buf, size_t len, int flags) {
  ssize_t     ret;
  Coroutine *coro = coros_[running_];
  epoll_event ev;

  memset(&ev, 0, sizeof(struct epoll_event));
  Socket *sock = socks_[fd];
  if (sock == NULL) {
    sock = new Socket();
    socks_[fd] = sock;
    sock->fd_ = fd;
    sock->coro_ = coro;
  }

  ev.data.ptr = (void*)sock;
  ev.events = EPOLLIN;
  if (epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &ev) != 0) {
    printf("recv add epoll error: %s\n", strerror(errno));
    return -1;
  }

  ret = 0;

swap:  
  coro->status_ = kStatusSuspend;
  swapcontext(&coro->ctx_, &main_);
  epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, &ev);

  while (ret < (ssize_t)len) {
    ssize_t nbytes = gSysRecv(fd, (char*)buf + ret, len - ret, flags);
    if (nbytes == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        goto swap;
      } else if (errno != EINTR) {
        return -1;
      } else {
        // EINTR
        continue;
      }
    }

    if (nbytes == 0) {
      return -1;
    }

    ret += nbytes;
    if (nbytes < (ssize_t)len - ret) {
      break;
    }
  }

  return ret;
}

ssize_t
Scheduler::Send(int fd, const void *buf, size_t len, int flags) {
  Coroutine *coro = coros_[running_];
  ssize_t     ret;

  epoll_event ev;
  memset(&ev, 0, sizeof(struct epoll_event));
  Socket *sock = socks_[fd];
  if (sock == NULL) {
    sock = new Socket();
    socks_[fd] = sock;
    sock->fd_ = fd;
    sock->coro_ = coro;
  }

  ev.data.ptr = (void*)sock;
  ev.events = EPOLLOUT;
  if (epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &ev) != 0) {
    return -1;
  }
  ret = 0;
swap:
  coro->status_ = kStatusSuspend;
  swapcontext(&coro->ctx_, &main_);
  epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, &ev);

  while (ret < (ssize_t)len) {
    ssize_t nbytes = gSysSend(fd, (char*)buf + ret, len - ret, flags | MSG_NOSIGNAL);
    if (nbytes == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        goto swap;
      } else if (errno != EINTR) {
        return -1;
      } else {
        // EINTR
        continue;
      }
    }

    if (nbytes == 0) {
      return -1;
    }

    ret += nbytes;
    if (ret == (ssize_t)len) {
      break;
    }
  }

  return ret;
}

int
Scheduler::Close(int fd) {
  if (socks_[fd]) {
    delete socks_[fd];
    socks_[fd] = NULL;
  }
  return gSysClose(fd);
}
