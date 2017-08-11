//#define _GNU_SOURCE
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/syscall.h>
#include <sys/select.h>
#include <sys/un.h>

#include <dlfcn.h>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>

#include <netinet/in.h>
#include <errno.h>
#include <time.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <pthread.h>

//#include <resolv.h>
#include <netdb.h>

#include <time.h>
#include "coroutine_impl.h"
#include "coroutine_task.h"
#include "coroutine_specific.h"

extern coroutine_options_t gOptions;

#define HOOK_SYS_FUNC(name) if( !g_sys_##name ) { g_sys_##name = (name##_fun_t)dlsym(RTLD_NEXT,#name); }

typedef int (*socket_fun_t)(int domain, int type, int protocol);
typedef int (*accept_fun_t)(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
typedef int (*connect_fun_t)(int socket, const struct sockaddr *address, socklen_t address_len);
typedef int (*close_fun_t)(int fd);

typedef int (*select_fun_t)(int nfds, fd_set *readfds, fd_set *writefds,
                           fd_set *exceptfds, struct timeval *timeout);

typedef ssize_t (*read_fun_t)(int fildes, void *buf, size_t nbyte);
typedef ssize_t (*write_fun_t)(int fildes, const void *buf, size_t nbyte);

typedef ssize_t (*sendto_fun_t)(int socket, const void *message, size_t length,
                     int flags, const struct sockaddr *dest_addr,
                                              socklen_t dest_len);

typedef ssize_t (*recvfrom_fun_t)(int socket, void *buffer, size_t length,
                     int flags, struct sockaddr *address,
                                              socklen_t *address_len);

typedef size_t (*send_fun_t)(int socket, const void *buffer, size_t length, int flags);
typedef ssize_t (*recv_fun_t)(int socket, void *buffer, size_t length, int flags);

typedef int (*setsockopt_fun_t)(int socket, int level, int option_name,
                         const void *option_value, socklen_t option_len);

typedef unsigned int (*sleep_fun_t)(unsigned int seconds);
typedef int (*usleep_fun_t)(useconds_t usec);
typedef int (*nanosleep_fun_t)(const struct timespec *req, struct timespec *rem);

typedef int (*fcntl_fun_t)(int fildes, int cmd, ...);
typedef struct tm *(*localtime_r_fun_t)( const time_t *timep, struct tm *result );

typedef void *(*pthread_getspecific_fun_t)(pthread_key_t key);
typedef int (*pthread_setspecific_fun_t)(pthread_key_t key, const void *value);

typedef int (*setenv_fun_t)(const char *name, const char *value, int overwrite);
typedef int (*unsetenv_fun_t)(const char *name);
typedef char *(*getenv_fun_t)(const char *name);
typedef struct hostent* (*gethostbyname_fun_t)(const char *name);
//typedef res_state (*__res_state_fun_t)();

// hook system functions
static socket_fun_t g_sys_socket;
static accept_fun_t g_sys_accept;
static connect_fun_t g_sys_connect;
static close_fun_t g_sys_close;

static select_fun_t g_sys_select;

static sleep_fun_t g_sys_sleep;
static usleep_fun_t g_sys_usleep;
static nanosleep_fun_t g_sys_nanosleep;

static read_fun_t g_sys_read;
static write_fun_t g_sys_write;

static send_fun_t g_sys_send;
static recv_fun_t g_sys_recv;

static poll_fun_t g_sys_poll;

static setsockopt_fun_t g_sys_setsockopt;
static fcntl_fun_t g_sys_fcntl;

static gethostbyname_fun_t g_sys_gethostbyname;

typedef struct rpchook_t {
	int user_flag;
	struct sockaddr_in dest; //maybe sockaddr_un;
	int domain; //AF_LOCAL , AF_INET

	struct timeval read_timeout;
	struct timeval write_timeout;
} rpchook_t;
static rpchook_t *gSocketFd[ 102400 ] = { 0 };

static inline char is_enable_sys_hook(coroutine_t *co) {
  if (!co || !co->task) {
    return 0;
  }
  return co->task->attr.enable_sys_hook;
}

static inline rpchook_t* get_by_fd(int fd) {
	if( fd > -1 && fd < (int)sizeof(gSocketFd) / (int)sizeof(gSocketFd[0]) ) {
		return gSocketFd[fd];
	}
	return NULL;
}

static inline rpchook_t * alloc_by_fd(int fd) {
	if( fd > -1 && fd < (int)sizeof(gSocketFd) / (int)sizeof(gSocketFd[0]) ) {
		rpchook_t *lp = (rpchook_t*)calloc(1,sizeof(rpchook_t));
		lp->read_timeout.tv_usec = 1000;
		lp->write_timeout.tv_usec = 1000;
		gSocketFd[fd] = lp;
		return lp;
	}
	return NULL;
}

static inline void free_by_fd(int fd) {
	if(fd > -1 && fd < (int)sizeof(gSocketFd) / (int)sizeof(gSocketFd[0])) {
		rpchook_t *lp = gSocketFd[fd];
		if(lp) {
			gSocketFd[fd] = NULL;
			free(lp);	
		}
	}
	return;
}


int socket(int domain, int type, int protocol) {
  HOOK_SYS_FUNC(socket);
	if(!is_enable_sys_hook(coroutine_self())) {
		return g_sys_socket(domain,type,protocol);
	}
	int fd = g_sys_socket(domain,type,protocol);
	if( fd < 0 ) {
		return fd;
	}

	rpchook_t *lp = alloc_by_fd(fd);
	lp->domain = domain;
	
  // set socket fd as non block by default
  HOOK_SYS_FUNC(fcntl);
	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL,0) | O_NONBLOCK);

	return fd;
}

int select(int nfds, fd_set *readfds, fd_set *writefds,
          fd_set *exceptfds, struct timeval *timeout) {
  HOOK_SYS_FUNC(select);
	if(!is_enable_sys_hook(coroutine_self())) {
	  return g_sys_select(nfds, readfds, writefds, exceptfds, timeout);
	}

  int timeout_ms = -1;
  if (timeout) {
    timeout_ms = timeout->tv_sec * 1000 + timeout->tv_usec / 1000;
  }

  if (timeout == 0) {
	  return g_sys_select(nfds, readfds, writefds, exceptfds, timeout);
  }

  fd_set rfs, wfs, efs;
  FD_ZERO(&rfs);
  FD_ZERO(&wfs);
  FD_ZERO(&efs);
  if (readfds) rfs = *readfds;
  if (writefds) wfs = *writefds;
  if (exceptfds) efs = *exceptfds;
  struct timeval zero_tv = {0, 0};
  int n = g_sys_select(nfds, (readfds ? &rfs : NULL),
    (writefds ? &wfs : NULL),
    (exceptfds ? &efs : NULL), &zero_tv);
  if (n != 0) {
    if (readfds) *readfds = rfs;
    if (writefds) *writefds = wfs;
    if (exceptfds) *exceptfds = efs;
    return n;
  }

  // convert select to poll
  // first compute total fd num
  fd_set* fds[3] = {readfds, writefds, exceptfds};
  int total = 0;
  for (int i = 0; i < 3; ++i) {
    fd_set *set = fds[i];
    if (!set) {
      continue;
    }
    for (int fd = 0; fd < nfds; ++fd) {
      if (FD_ISSET(fd, set)) {
        total++;
      }
    }
  }

  // allocate poll array
  struct pollfd *poll_fds = (struct pollfd*)calloc(total, sizeof(struct pollfd));
  uint32_t poll_events[3] = {POLLIN, POLLOUT, 0};
  for (int i = 0; i < 3; ++i) {
    fd_set *set = fds[i];
    if (!set) {
      continue;
    }
    for (int fd = 0; fd < nfds; ++fd) {
      if (FD_ISSET(fd, set)) {
        poll_fds[i].fd = fd;
        poll_fds[i].events = poll_events[i];
      }
    }
  }

  // OK, do the poll work
  n = poll(poll_fds, total, timeout_ms);
  if (n <= 0) {
    goto out;
  }

  // convert pollfd to fd_set
  int ret = 0;
  for (int i = 0; i < total; ++i) {
    struct pollfd *pfd = &poll_fds[i];
    if ((pfd->events & POLLIN) && readfds) {
      FD_SET(pfd->fd, readfds);
      ++ret;
    }

    if ((pfd->events & POLLOUT) && writefds) {
      FD_SET(pfd->fd, writefds);
      ++ret;
    }

    if ((pfd->events & ~(POLLIN | POLLOUT)) && exceptfds) {
      FD_SET(pfd->fd, exceptfds);
      ++ret;
    }
  }

out:  
  free(poll_fds);
  return ret;
}

int accept(int listen_fd, struct sockaddr *addr, socklen_t *len) {
  HOOK_SYS_FUNC(accept);
  coroutine_t *co = coroutine_self();
	if(!is_enable_sys_hook(co)) {
	  return g_sys_accept(listen_fd,addr,len);
	}
	rpchook_t *lp = get_by_fd(listen_fd);
	if(!lp) {
    alloc_by_fd(listen_fd);
	  fcntl(listen_fd, F_SETFL, fcntl(listen_fd, F_GETFL,0) | O_NONBLOCK);
  }
	int fd = g_sys_accept(listen_fd,addr,len);

  do {
    if(fd < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        struct pollfd pf = {0};
        pf.fd = listen_fd;
        pf.events = (POLLIN|POLLERR|POLLHUP);
        int n = coroutine_poll(get_epoll_context(),&pf,1, -1);
        if (n <= 0 && co->task->timeout) {
          close(listen_fd);
          errno = EBADF;
          return -1;
        }
	      fd = g_sys_accept(listen_fd,addr,len);
        continue;
      }
      return fd;
    }
  } while (fd < 0);

	lp = alloc_by_fd(fd);

  int current_flags = fcntl(fd ,F_GETFL, 0);
  int flag = current_flags;
  flag |= O_NONBLOCK;

  // set socket fd as non block by default
  int ret = fcntl(fd ,F_SETFL, flag);
  if (0 == ret && lp) {
    lp->user_flag = current_flags;
  }

	return fd;
}

int connect(int fd, const struct sockaddr *address, socklen_t address_len) {
  HOOK_SYS_FUNC(connect);
  coroutine_t *co = coroutine_self();
	if(!is_enable_sys_hook(co)) {
		return g_sys_connect(fd,address,address_len);
	}

	//1.sys call
	int ret = g_sys_connect(fd,address,address_len);

	rpchook_t *lp = get_by_fd(fd);
	if(!lp) {
    return ret;
  }

	if(sizeof(lp->dest) >= address_len) {
		 memcpy(&(lp->dest),address,(int)address_len);
	}

  /*
	if(O_NONBLOCK & lp->user_flag) {
		return ret;
	}
	*/

	if (!(ret < 0 && errno == EINPROGRESS)) {
		return ret;
	}

	//2.wait
	struct pollfd pf = { 0 };

  memset(&pf,0,sizeof(pf));
  pf.fd = fd;
  pf.events = (POLLOUT | POLLERR | POLLHUP);

  if(!poll(&pf,1,-1) && co->task->timeout) {
    close(fd);
    errno = EBADF;
    return -1;
  }

	if(pf.revents & POLLOUT) {
		errno = 0;
		return 0;
	}

/*  
	//3.set errno
	int err = 0;
	socklen_t errlen = sizeof(err);
	getsockopt(fd,SOL_SOCKET,SO_ERROR,&err,&errlen);
	if(err) {
		errno = err;
	} else {
		errno = ETIMEDOUT;
	} 
*/  
	return ret;
}

int close(int fd) {
  HOOK_SYS_FUNC(close);
	if(!is_enable_sys_hook(coroutine_self())) {
		return g_sys_close(fd);
	}

  if (get_by_fd(fd) != NULL) {
  	free_by_fd(fd);
  }
	return g_sys_close(fd);
}

// 1: io ready
// -1: timeout
static int wait_io_ready(coroutine_t *co, int fd, int events, int timeout) {
  struct pollfd pf = {0};
  pf.fd = fd;
  pf.events = (POLLIN | POLLERR | POLLHUP);

  if(!poll(&pf,1,timeout) && co->task->timeout) {
    close(fd);
    errno = EBADF;
    return -1;
  }

  return 1;
}

ssize_t read(int fd, void *buf, size_t nbyte) {
  HOOK_SYS_FUNC(read);
  coroutine_t *co = coroutine_self();
	if(!is_enable_sys_hook(co)) {
		return g_sys_read(fd,buf,nbyte);
	}

	rpchook_t *lp = get_by_fd(fd);

	if(!lp) {
		return g_sys_read(fd,buf,nbyte);
  }

  int timeout = (lp->read_timeout.tv_sec * 1000) + (lp->read_timeout.tv_usec / 1000);

  ssize_t n = 0;
  int ready;
  do {
    ready = wait_io_ready(co, fd, POLLIN | POLLERR | POLLHUP, timeout);
    if (ready <= 0) {
      break;
    }

	  ssize_t ret = g_sys_read(fd, buf + n, nbyte - n);
    if (ret < 0) {
      if (errno != EAGAIN && errno != EWOULDBLOCK) {
        break;
      }
      continue;
    } else if (ret == 0) {
      break;
    }

    n += ret;
  } while (n <= 0 && n < nbyte);
  if (ready <= 0 && n == 0) {
    return ready;
  }
  return n;
}

ssize_t write(int fd, const void *buf, size_t nbyte) {
  HOOK_SYS_FUNC(write);
  coroutine_t *co = coroutine_self();
	if(!is_enable_sys_hook(co)) {
		return g_sys_write(fd,buf,nbyte);
	}

	rpchook_t *lp = get_by_fd(fd);

	if(!lp) {
		return g_sys_write(fd,buf,nbyte);
	}

  int timeout = (lp->write_timeout.tv_sec * 1000) + (lp->write_timeout.tv_usec / 1000);
  ssize_t n = 0;
  ssize_t ret = g_sys_write(fd, buf, nbyte);
  if (ret == 0) {
    return ret;
  }

  if (ret > 0) {
    n += ret;
  }

  while (n < nbyte) {
    ret = wait_io_ready(co, fd, POLLOUT | POLLERR | POLLHUP, timeout);
    if (ret <= 0) {
      break;
    }

    ret = g_sys_write(fd, buf + n, nbyte - n);

    if (ret < 0) {
      if (errno != EAGAIN && errno != EWOULDBLOCK) {
        break;
      }
      continue;
    } else if (ret == 0) {
      break;
    }

    n += ret;
  }

  if (ret <= 0 && n == 0) {
    return ret;
  }
  return n;
}

ssize_t send(int socket, const void *buffer, size_t length, int flags) {
  HOOK_SYS_FUNC(send);
  coroutine_t *co = coroutine_self();
	if(!is_enable_sys_hook(co)) {
		return g_sys_send(socket,buffer,length,flags);
	}

	rpchook_t *lp = get_by_fd(socket);

	if( !lp) {
		return g_sys_send(socket,buffer,length,flags);
	}

  int timeout = (lp->write_timeout.tv_sec * 1000) + (lp->write_timeout.tv_usec / 1000);
  ssize_t n = 0;
  ssize_t ret = g_sys_send(socket, buffer, length, flags);
  if (ret == 0) {
    return ret;
  }

  if (ret > 0) {
    n += ret;
  }

  while (n < length) {
    ret = wait_io_ready(co, socket, POLLOUT | POLLERR | POLLHUP, timeout);
    if (ret <= 0) {
      break;
    }

    ret = g_sys_send(socket, buffer + n, length - n, flags);
    if (ret < 0) {
      if (errno != EAGAIN && errno != EWOULDBLOCK) {
        break;
      }
      continue;
    } else if (ret == 0) {
      break;
    }
    n += ret;
  }

  if (ret <= 0 && n == 0) {
    return ret;
  }
  return n;
}

ssize_t recv(int socket, void *buffer, size_t length, int flags) {
  HOOK_SYS_FUNC(recv);
  coroutine_t *co = coroutine_self();
	if(!is_enable_sys_hook(co)) {
		return g_sys_recv(socket,buffer,length,flags);
	}
	rpchook_t *lp = get_by_fd(socket);

	if(!lp) {
		return g_sys_recv(socket,buffer,length,flags);
	}

  int timeout = (lp->read_timeout.tv_sec * 1000) + (lp->read_timeout.tv_usec / 1000);

  ssize_t n = 0;
  int ready = 0;

  do {
    ready = wait_io_ready(co, socket, POLLIN | POLLERR | POLLHUP, timeout);
    if (ready <= 0) {
      break;
    }

    ssize_t ret = g_sys_recv(socket, buffer + n, length - n, flags);
    if (ret < 0) {
      if (errno != EAGAIN && errno != EWOULDBLOCK) {
        break;
      }
      continue;
    } else if (ret == 0) {
      break;
    }

    n += ret;
  } while (n <= 0 && n < length);
  if (ready <= 0 && n == 0) {
    return ready;
  }
  return n;
}

int poll(struct pollfd fds[], nfds_t nfds, int timeout) {
  HOOK_SYS_FUNC(poll);
  coroutine_t *co = coroutine_self();
	if(!is_enable_sys_hook(co)) {
		return g_sys_poll(fds,nfds,timeout);
	}

	return poll_inner(get_epoll_context(),fds,nfds,timeout, g_sys_poll);
}

int __poll(struct pollfd fds[], nfds_t nfds, int timeout) {
  return poll(fds, nfds, timeout);
}

static void doSleep(unsigned long long timeout_ms) {
  struct pollfd fds[1];

  poll_inner(get_epoll_context(), fds, 0, timeout_ms, NULL);
}

unsigned int sleep(unsigned int seconds) {
  HOOK_SYS_FUNC(sleep);
  coroutine_t *co = coroutine_self();
	if(!is_enable_sys_hook(co)) {
		return g_sys_sleep(seconds);
	}

  doSleep(seconds * 1000);
  return 0;
}

int usleep(useconds_t usec) {
  HOOK_SYS_FUNC(usleep);
  coroutine_t *co = coroutine_self();
	if(!is_enable_sys_hook(co)) {
		return g_sys_usleep(usec);
	}

  doSleep(usec / 1000);

  return 0;
}

int nanosleep(const struct timespec *req, struct timespec *rem) {
  HOOK_SYS_FUNC(nanosleep);
  coroutine_t *co = coroutine_self();
	if(!is_enable_sys_hook(co)) {
		return g_sys_nanosleep(req, rem);
	}
  int timeout_ms = req->tv_sec * 1000 + req->tv_nsec / 1000000;

  doSleep(timeout_ms);

  return 0;
}

int setsockopt(int fd, int level, int option_name,
			                 const void *option_value, socklen_t option_len) {
  HOOK_SYS_FUNC(setsockopt);
  coroutine_t *co = coroutine_self();
	if(!is_enable_sys_hook(co)) {
		return g_sys_setsockopt(fd,level,option_name,option_value,option_len);
	}

	rpchook_t *lp = get_by_fd(fd);

	if(lp && SOL_SOCKET == level) {
		struct timeval *val = (struct timeval*)option_value;
		if(SO_RCVTIMEO == option_name) {
			memcpy(&lp->read_timeout,val,sizeof(*val));
		} else if( SO_SNDTIMEO == option_name ) {
			memcpy(&lp->write_timeout,val,sizeof(*val));
		}
	}
	return g_sys_setsockopt(fd,level,option_name,option_value,option_len);
}

int fcntl(int fildes, int cmd, ...) {
  HOOK_SYS_FUNC(fcntl);
	if(fildes < 0) {
		return -1;
	}

	va_list arg_list;
	va_start(arg_list,cmd);

	int ret = -1;
	rpchook_t *lp = get_by_fd(fildes);
	switch(cmd) {
		case F_DUPFD: {
			int param = va_arg(arg_list,int);
			ret = g_sys_fcntl(fildes,cmd,param);
			break;
		}
		case F_GETFD: {
			ret = g_sys_fcntl(fildes,cmd);
			break;
		}
		case F_SETFD: {
			int param = va_arg(arg_list,int);
			ret = g_sys_fcntl(fildes,cmd,param);
			break;
		}
		case F_GETFL: {
			ret = g_sys_fcntl(fildes,cmd);
			break;
		}
		case F_SETFL: {
			int param = va_arg(arg_list,int);
			int flag = param;
      // set as non block by default
			if(is_enable_sys_hook(coroutine_self()) && lp) {
				flag |= O_NONBLOCK;
			}
			ret = g_sys_fcntl(fildes,cmd,flag);
			if(0 == ret && lp) {
				lp->user_flag = param;
			}
			break;
		}
		case F_GETOWN: {
			ret = g_sys_fcntl(fildes,cmd);
			break;
		}
		case F_SETOWN: {
			int param = va_arg(arg_list,int);
			ret = g_sys_fcntl(fildes,cmd,param);
			break;
		}
		case F_GETLK: {
			struct flock *param = va_arg(arg_list,struct flock *);
			ret = g_sys_fcntl(fildes,cmd,param);
			break;
		}
		case F_SETLK: {
			struct flock *param = va_arg(arg_list,struct flock *);
			ret = g_sys_fcntl(fildes,cmd,param);
			break;
		}
		case F_SETLKW: {
			struct flock *param = va_arg(arg_list,struct flock *);
			ret = g_sys_fcntl(fildes,cmd,param);
			break;
		}
	}

	va_end(arg_list);

	return ret;
}

typedef struct hostbuf_wrap {
	struct hostent host;
	char* buffer;
	size_t buffer_size;
	int host_errno;
} hostbuf_wrap;

struct hostent *coroutine_gethostbyname(const char *name) {
	if (!name) {
		return NULL;
	}

  hostbuf_wrap *__co_hostbuf_wrap = (hostbuf_wrap*)coroutine_getspecific(gCoroutineHostbufKey);
  if (__co_hostbuf_wrap == NULL) {
    __co_hostbuf_wrap = (hostbuf_wrap*)calloc(1, sizeof(hostbuf_wrap));
    int ret = coroutine_setspecific(gCoroutineHostbufKey, __co_hostbuf_wrap);
    if (ret != 0) {
      free(__co_hostbuf_wrap);
      __co_hostbuf_wrap = NULL;
    }
  }

	if (__co_hostbuf_wrap->buffer && __co_hostbuf_wrap->buffer_size > 1024) {
		free(__co_hostbuf_wrap->buffer);
		__co_hostbuf_wrap->buffer = NULL;
	}
	if (!__co_hostbuf_wrap->buffer) {
		__co_hostbuf_wrap->buffer = (char*)malloc(1024);
		__co_hostbuf_wrap->buffer_size = 1024;
	}

	struct hostent *host = &__co_hostbuf_wrap->host;
	struct hostent *result = NULL;
	int *h_errnop = &(__co_hostbuf_wrap->host_errno);

	int ret = -1;
	while ((ret = gethostbyname_r(name, host, __co_hostbuf_wrap->buffer, 
				__co_hostbuf_wrap->buffer_size, &result, h_errnop)) == ERANGE && 
				(*h_errnop == NETDB_INTERNAL)) {
		free(__co_hostbuf_wrap->buffer);
		__co_hostbuf_wrap->buffer_size = __co_hostbuf_wrap->buffer_size * 2;
		__co_hostbuf_wrap->buffer = (char*)malloc(__co_hostbuf_wrap->buffer_size);
	}

	if (ret == 0 && (host == result)) {
		return host;
	}
	return NULL;
}

struct hostent *gethostbyname(const char *name) {
  if (!is_enable_sys_hook(coroutine_self())) {
    return g_sys_gethostbyname(name);
  }
  return coroutine_gethostbyname(name);
}
