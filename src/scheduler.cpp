#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
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

  current_ = NewCoroutineId();
  main_ = new Coroutine(this, current_, ScheduleMain, this);
  coros_[current_] = main_;

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
  return main_->get_context();
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

  active_.push_back(coro);
  ucontext_t *context = coro->get_context();
  current_ = id;
  swapcontext(main_->get_context(), context);

  return id;
}

int
Scheduler::Yield(int id) {
  return 0;
}

int
Scheduler::Resume(int id) {
  return 0;
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

int
Scheduler::Listen(int fd) {
  Coroutine *coro = coros_[current_];

  epoll_event ev;
  Socket *sock = new Socket();
  sock->fd = fd;
  sock->co = coro;
  socks_[fd] = sock;
  ev.data.ptr = (void*)sock;
  ev.events = EPOLLIN;
  if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) != 0) {
    return -1;
  }
  coro->set_status(SUSPEND);
  swapcontext(coro->get_context(), main_->get_context());
  epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, &ev);

  return 1;
}

ssize_t
Scheduler::Recv(int fd, void *buf, size_t len, int flags) {
  Coroutine *coro = coros_[current_];
  ssize_t     ret;

  epoll_event ev;
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
  swapcontext(coro->get_context(), main_->get_context());
  epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, &ev);

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
  swapcontext(coro->get_context(), main_->get_context());
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
  int nfds = epoll_wait(epoll_fd_, events, sizeof(events), 100);
  int i;

  for(i = 0 ; i < nfds ; ++i) {
    if(events[i].events & EPOLLIN)
    {     
      sock = (Socket*)events[i].data.ptr;
      sock->co->set_status(RUNNING);
      active_.push_back(sock->co); 
      continue;
    }
    if(events[i].events & EPOLLOUT)
    {
      sock = (Socket*)events[i].data.ptr;
      sock->co->set_status(RUNNING);
      active_.push_back(sock->co); 
      continue;
    }
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
  ucontext_t *main;
  int id;

  main = sched->get_context();
  while (true) {
    for (iter = sched->active_.begin();
         iter != sched->active_.end(); ) {
      coro = *iter;
      id = coro->get_id();
      sched->current_ = id;

      swapcontext(main,coro->get_context()); 
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
