#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <dlfcn.h>
#include "coroutine.h"
#include "scheduler.h"

void* ScheduleMain(void *);

Scheduler::Scheduler()
  : epoll_fd_(-1),
    coros_(NULL),
    capacity_(kCoroNum),
    num_(0),
    current_(-1),
    last_(0) {
  coros_ = (Coroutine**)malloc(sizeof(Coroutine*) * capacity_);
  memset(coros_, 0, sizeof(Coroutine*) * capacity_);

  /*
  current_ = NewCoroutineId();
  //main_ = new Coroutine(this, current_, ScheduleMain, this);
  main_ = new Coroutine(this, current_, NULL, this);
  coros_[current_] = main_;
  printf("current: %d\n", current_);
  */

  epoll_fd_ = epoll_create(1024);
}

Scheduler::~Scheduler() {
  free(coros_);
}

int
Scheduler::NewCoroutineId() {
  if (num_ >= capacity_) {
    capacity_ *= 2;
    coros_ = (Coroutine**)realloc(coros_, capacity_);
    if (coros_ == NULL) {
      return -1;
    }
  }

  int id, i;
  for (i = 0; i < capacity_; ++i) {
    id = (i + last_) % capacity_;
    if (coros_[id] == NULL) {
      last_ = id;
      ++num_;
      return id;
    }
  }

  // TODO:check here
  return -1;
}

ucontext_t *
Scheduler::get_context() {
  return &main_;
}

int
Scheduler::Spawn(cfunc func, void *arg) {
  int id;

  id = NewCoroutineId();
  if (id == -1) {
    return -1;
  }

  Coroutine *coro = new Coroutine(this, id, func, arg);
  if (!coro) {
    return -1;
  }
  coros_[id] = coro;

  printf("Spawn: %d\n", id);
  active_.push_back(coro);
  ucontext_t *context = coro->get_context();
  current_ = id;
  //swapcontext(main_->get_context(), context);
  swapcontext(&main_, context);

  return id;
}

unsigned int
Scheduler::Sleep(unsigned int seconds) {
  Coroutine *coro = coros_[current_];
  time_t now = time(NULL);
  sleep_.insert(make_pair((now + seconds) * 1000, coro));
  coro->set_status(SUSPEND);
  swapcontext(coro->get_context(), get_context());

  return 0;
}

typedef int (*myAccept)(int, struct sockaddr *, socklen_t *);

int
Scheduler::Accept(int fd, struct sockaddr *addr, socklen_t *addrlen) {
  Coroutine *coro = coros_[current_];

  epoll_event ev;
  memset(&ev, 0, sizeof(struct epoll_event));
  Socket *sock = socks_[fd];
  if (sock == NULL) {
    sock = new Socket();
    socks_[fd] = sock;
  } else {
    sock->fd = fd;
    sock->co = coro;
    socks_[fd] = sock;
  }
  ev.data.ptr = (void*)sock;
  ev.events = EPOLLIN;

  printf("accept fd:%d\n", fd);
  if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) != 0) {
    printf("epoll_ctl error:%s\n", strerror(errno));
    //return -1;
  }
  coro->set_status(SUSPEND);
  printf("before:%d, sock: %p\n", current_, sock);
  swapcontext(coro->get_context(), &main_);
  printf("after accept\n");
  epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, &ev);

  static myAccept my_accept = (myAccept)dlsym(RTLD_NEXT, "accept");
  //return syscall(SYS_accept, fd, addr, addrlen);
  int s = my_accept(fd, addr, addrlen);
  printf("accept: %d\n", s);

  return s;
}

ssize_t
Scheduler::Recv(int fd, void *buf, size_t len, int flags) {
  Coroutine *coro = coros_[current_];
  ssize_t     ret;

  epoll_event ev;
  memset(&ev, 0, sizeof(struct epoll_event));
  Socket *sock = socks_[fd];
  if (sock == NULL) {
    sock = new Socket();
    socks_[fd] = sock;
  }
  sock->fd = fd;
  sock->co = coro;

  ev.data.ptr = (void*)sock;
  ev.events = EPOLLIN;
  if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) != 0) {
    return -1;
  }
  coro->set_status(SUSPEND);
  printf("before recv:%p\n", buf);
  swapcontext(coro->get_context(), &main_);
  epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, &ev);

  printf("after recv:%p\n", buf);
  ret = 0;
  while (ret < (ssize_t)len) {
    ssize_t nbytes = recv(fd, (char*)buf + ret, len - ret, flags);
    if (nbytes == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        break;
      } else if (errno != EINTR) {
        return -1;
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
  Coroutine *coro = coros_[current_];
  ssize_t     ret;

  epoll_event ev;
  memset(&ev, 0, sizeof(struct epoll_event));
  Socket *sock = socks_[fd];
  if (sock == NULL) {
    sock = new Socket();
    socks_[fd] = sock;
  }
  sock->fd = fd;
  sock->co = coro;

  ev.data.ptr = (void*)sock;
  ev.events = EPOLLOUT;
  if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) != 0) {
    return -1;
  }
  coro->set_status(SUSPEND);
  swapcontext(coro->get_context(), &main_);
  epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, &ev);

  ret = 0;
  while (ret < (ssize_t)len) {
    ssize_t nbytes = send(fd, (char*)buf + ret, len - ret, flags | MSG_NOSIGNAL);
    if (nbytes == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        break;
      } else if (errno != EINTR) {
        return -1;
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

void
Scheduler::CheckNetwork() {
  epoll_event events[1024];
  Socket *sock;
  int nfds = epoll_wait(epoll_fd_, events, sizeof(events), -1);
  int i;

  if (nfds > 0) { 
    for(i = 0 ; i < nfds ; ++i) {
      if(events[i].events & EPOLLIN) {     
        sock = (Socket*)events[i].data.ptr;
        sock->co->set_status(RUNNING);
        active_.push_back(sock->co); 
        continue;
      }
      if(events[i].events & EPOLLOUT) {
        sock = (Socket*)events[i].data.ptr;
        sock->co->set_status(RUNNING);
        active_.push_back(sock->co); 
        continue;
      }
    }
  } else {
    printf("epoll error: %s\n", strerror(errno));
  }
}

void
Scheduler::Run() {
  ScheduleMain(this);
}

void*
ScheduleMain(void *arg) {
  Scheduler *sched = (Scheduler*)arg;
  list<Coroutine*>::iterator iter, tmp;
  Coroutine *coro;
  ucontext_t *main, *ucont;
  int id;

  main = sched->get_context();
  while (true) {
    printf("in ScheduleMain\n");
    for (iter = sched->active_.begin();
         iter != sched->active_.end();) {
      coro = *iter;
      id = coro->get_id();
      sched->current_ = id;

      printf("!!!swap: %d\n", id);

      ucont = coro->get_context();
      swapcontext(main, coro->get_context()); 
      printf("after swap: %d\n", id);
      int status = coro->get_status();
      if (status == DEAD) {
        delete coro;
        tmp = iter;
        ++iter;
        sched->active_.erase(tmp);
        sched->coros_[id] = NULL;
        --sched->num_;
      } else if (status == SUSPEND) {
        tmp = iter;
        ++iter;
        sched->active_.erase(tmp);
      } else {
        ++iter;
      }
    }

    sched->CheckNetwork();
  }

  return NULL;
}
