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
#include "coroutine_specific.h"

extern coroutine_options_t gOptions;

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

typedef int (*poll_fun_t)(struct pollfd fds[], nfds_t nfds, int timeout);
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
typedef int (*__poll_fun_t)(struct pollfd fds[], nfds_t nfds, int timeout);

// hook system functions
static socket_fun_t gSysSocket;
static accept_fun_t gSysAccept;
static connect_fun_t gSysConnect;
static close_fun_t gSysClose;
static select_fun_t gSysSelect;

static read_fun_t gSysRead;
static write_fun_t gSysWrite;

static sendto_fun_t sSysSendto;
static recvfrom_fun_t gSysRecvFrom;

static send_fun_t gSysSend;
static recv_fun_t gSysRecv;

static poll_fun_t gSysPoll;
static sleep_fun_t gSysSleep;
static usleep_fun_t gSysUsleep;
static nanosleep_fun_t gSysNanosleep;

static setsockopt_fun_t gSysSetsockopt;
static fcntl_fun_t gSysFcntl;

static gethostbyname_fun_t gSysgethostbyname;

typedef struct rpchook_t {
	int user_flag;
	struct sockaddr_in dest; //maybe sockaddr_un;
	int domain; //AF_LOCAL , AF_INET

	struct timeval read_timeout;
	struct timeval write_timeout;
} rpchook_t;
static rpchook_t *gSocketFd[ 102400 ] = { 0 };

static inline int getMaxIoTimeoutMs() {
  return gOptions.max_io_timeout_ms;
}

static inline char isEnableSysHook() {
  return gOptions.enable_sys_hook;
}

static inline rpchook_t* getByFd(int fd) {
	if( fd > -1 && fd < (int)sizeof(gSocketFd) / (int)sizeof(gSocketFd[0]) ) {
		return gSocketFd[fd];
	}
	return NULL;
}

static inline rpchook_t * allocByFd(int fd) {
	if( fd > -1 && fd < (int)sizeof(gSocketFd) / (int)sizeof(gSocketFd[0]) ) {
		rpchook_t *lp = (rpchook_t*)calloc(1,sizeof(rpchook_t));
		lp->read_timeout.tv_sec = getMaxIoTimeoutMs() * 1000;
		lp->write_timeout.tv_sec = getMaxIoTimeoutMs() * 1000;
		gSocketFd[ fd ] = lp;
		return lp;
	}
	return NULL;
}

static inline void freeByFd(int fd) {
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
	if(!isEnableSysHook()) {
		return gSysSocket(domain,type,protocol);
	}
	int fd = gSysSocket(domain,type,protocol);
	if( fd < 0 ) {
		return fd;
	}

	rpchook_t *lp = allocByFd(fd);
	lp->domain = domain;
	
  // set socket fd as non block by default
	fcntl(fd, F_SETFL, gSysFcntl(fd, F_GETFL,0) | O_NONBLOCK);

	return fd;
}

int select(int nfds, fd_set *readfds, fd_set *writefds,
          fd_set *exceptfds, struct timeval *timeout) {
	if(!isEnableSysHook()) {
	  return gSysSelect(nfds, readfds, writefds, exceptfds, timeout);
	}

  int timeout_ms = -1;
  if (timeout) {
    timeout_ms = timeout->tv_sec * 1000 + timeout->tv_usec / 1000;
  }

  if (timeout == 0) {
	  return gSysSelect(nfds, readfds, writefds, exceptfds, timeout);
  }

  fd_set rfs, wfs, efs;
  FD_ZERO(&rfs);
  FD_ZERO(&wfs);
  FD_ZERO(&efs);
  if (readfds) rfs = *readfds;
  if (writefds) wfs = *writefds;
  if (exceptfds) efs = *exceptfds;
  struct timeval zero_tv = {0, 0};
  int n = gSysSelect(nfds, (readfds ? &rfs : NULL),
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
  struct pollfd *poll_fds = (struct pollfd*)malloc(total * sizeof(struct pollfd));
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
  n = 0;
  for (int i = 0; i < total; ++i) {
    struct pollfd *pfd = &poll_fds[i];
    if ((pfd->events & POLLIN) && readfds) {
      FD_SET(pfd->fd, readfds);
      ++n;
    }

    if ((pfd->events & POLLOUT) && writefds) {
      FD_SET(pfd->fd, writefds);
      ++n;
    }

    if ((pfd->events & ~(POLLIN | POLLOUT)) && exceptfds) {
      FD_SET(pfd->fd, exceptfds);
      ++n;
    }
  }

out:  
  free(poll_fds);
  return n;
}

int accept(int listen_fd, struct sockaddr *addr, socklen_t *len) {
	if(!isEnableSysHook()) {
	  return gSysAccept(listen_fd,addr,len);
	}
	int fd = gSysAccept(listen_fd,addr,len);

  do {
    if(fd < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        struct pollfd pf = {0};
        pf.fd = listen_fd;
        pf.events = (POLLIN|POLLERR|POLLHUP);
        coroutine_poll(get_epoll_context(),&pf,1,getMaxIoTimeoutMs());
	      fd = gSysAccept(listen_fd,addr,len);
        continue;
      }
      printf("here: %d\n", errno);
      return fd;
    }
  } while (fd < 0);

	rpchook_t *lp = allocByFd(fd);

  int current_flags = gSysFcntl(fd ,F_GETFL, 0);
  int flag = current_flags;
  flag |= O_NONBLOCK;

  // set socket fd as non block by default
  int ret = gSysFcntl(fd ,F_SETFL, flag);
  if (0 == ret && lp) {
    lp->user_flag = current_flags;
  }

	return fd;
}

int connect(int fd, const struct sockaddr *address, socklen_t address_len) {
	if(!isEnableSysHook()) {
		return gSysConnect(fd,address,address_len);
	}

	//1.sys call
	int ret = gSysConnect(fd,address,address_len);

	rpchook_t *lp = getByFd(fd);
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
	int pollret = 0;
	struct pollfd pf = { 0 };

	for(int i=0;i<3;i++) {
		memset( &pf,0,sizeof(pf) );
		pf.fd = fd;
		pf.events = (POLLOUT | POLLERR | POLLHUP);

		pollret = poll(&pf ,1, getMaxIoTimeoutMs());

		if(1 == pollret) {
			break;
		}
	}
	if( pf.revents & POLLOUT ) {
		errno = 0;
		return 0;
	}

	//3.set errno
	int err = 0;
	socklen_t errlen = sizeof(err);
	getsockopt(fd,SOL_SOCKET,SO_ERROR,&err,&errlen);
	if(err) {
		errno = err;
	} else {
		errno = ETIMEDOUT;
	} 
	return ret;
}

int close(int fd) {
	if(!isEnableSysHook()) {
		return gSysClose(fd);
	}

	freeByFd(fd);
	return gSysClose(fd);
}

ssize_t read(int fd, void *buf, size_t nbyte) {
	if(!isEnableSysHook()) {
		return gSysRead(fd,buf,nbyte);
	}

	rpchook_t *lp = getByFd(fd);

	if(!lp) {
		return gSysRead(fd,buf,nbyte);
  }

  ssize_t n = 0;
  do {
		ssize_t ret = gSysRead(fd, buf + n, nbyte - n);
    if (ret == 0) {
      return n;
    }

    if (ret > 0) {
      n += ret;
      continue;
    }

    // at here means ret < 0
    if (n > 0) {
      return n;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      struct pollfd pf = { 0 };
      pf.fd = fd;
      pf.events = (POLLIN | POLLERR | POLLHUP);
      int timeout = (lp->read_timeout.tv_sec * 1000 ) 
        + ( lp->read_timeout.tv_usec / 1000 );

      poll(&pf,1,timeout);
    }
  } while(n < nbyte);

  return n;
}

ssize_t write(int fd, const void *buf, size_t nbyte) {
	if(!isEnableSysHook()) {
		return gSysWrite(fd,buf,nbyte);
	}

	rpchook_t *lp = getByFd(fd);

	if(!lp) {
		return gSysWrite(fd,buf,nbyte);
	}

  ssize_t n = 0;
  do {
		ssize_t ret = gSysWrite(fd, buf + n, nbyte - n);
    if (ret == 0) {
      return n;
    }

    if (ret > 0) {
      n += ret;
      continue;
    }

    // at here means ret < 0
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      struct pollfd pf = { 0 };
      pf.fd = fd;
      pf.events = (POLLIN | POLLERR | POLLHUP);
      int timeout = ( lp->read_timeout.tv_sec * 1000 ) 
        + ( lp->read_timeout.tv_usec / 1000 );

      poll(&pf,1,timeout);
    }
  } while(n < nbyte);

  return n;
}

ssize_t send(int socket, const void *buffer, size_t length, int flags) {
	if(!isEnableSysHook()) {
		return gSysSend(socket,buffer,length,flags);
	}

	rpchook_t *lp = getByFd(socket);

	if( !lp) {
		return gSysSend(socket,buffer,length,flags);
	}

  ssize_t n = 0;
  do {
		ssize_t ret = gSysSend(socket, buffer + n, length - n, flags);
    if (ret == 0) {
      return n;
    }

    if (ret > 0) {
      n += ret;
      continue;
    }

    // at here means ret < 0
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      struct pollfd pf = { 0 };
      pf.fd = socket;
      pf.events = (POLLIN | POLLERR | POLLHUP);
      int timeout = ( lp->read_timeout.tv_sec * 1000 ) 
        + ( lp->read_timeout.tv_usec / 1000 );

      poll(&pf,1,timeout);
    }
  } while(n < length);

  return n;
}

ssize_t recv(int socket, void *buffer, size_t length, int flags) {
	if(!isEnableSysHook()) {
		return gSysRecv(socket,buffer,length,flags);
	}
	rpchook_t *lp = getByFd(socket);

	if(!lp) {
		return gSysRecv( socket,buffer,length,flags );
	}

  ssize_t n = 0;
  do {
		ssize_t ret = gSysRecv(socket, buffer + n, length - n, flags);
    if (ret == 0) {
      return n;
    }

    if (ret > 0) {
      n += ret;
      continue;
    }

    // at here means ret < 0
    if (n > 0) {
      return n;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      struct pollfd pf = { 0 };
      pf.fd = socket;
      pf.events = (POLLIN | POLLERR | POLLHUP);
      int timeout = ( lp->read_timeout.tv_sec * 1000 ) 
        + ( lp->read_timeout.tv_usec / 1000 );

      poll(&pf,1,timeout);
    }
  } while(n < length);
	
  return n;
}

int poll(struct pollfd fds[], nfds_t nfds, int timeout) {
	if(!isEnableSysHook()) {
		return gSysPoll(fds,nfds,timeout);
	}

	return poll_inner(get_epoll_context(),fds,nfds,timeout, gSysPoll);
}

static void doSleep(unsigned long long timeout_ms) {
  struct pollfd fds[1];

  poll_inner(get_epoll_context(), fds, 0, timeout_ms, NULL);
}

unsigned int sleep(unsigned int seconds) {
	if(!isEnableSysHook()) {
		return gSysSleep(seconds);
	}

  doSleep(seconds * 1000);
  return 0;
}

int usleep(useconds_t usec) {
	if(!isEnableSysHook()) {
		return gSysUsleep(usec);
	}

  doSleep(usec / 1000);

  return 0;
}

int nanosleep(const struct timespec *req, struct timespec *rem) {
	if(!isEnableSysHook()) {
		return gSysNanosleep(req, rem);
	}
  int timeout_ms = req->tv_sec * 1000 + req->tv_nsec / 1000000;

  doSleep(timeout_ms);

  return 0;
}

int setsockopt(int fd, int level, int option_name,
			                 const void *option_value, socklen_t option_len) {
	if(!isEnableSysHook()) {
		return gSysSetsockopt(fd,level,option_name,option_value,option_len);
	}

	rpchook_t *lp = getByFd(fd);

	if(lp && SOL_SOCKET == level) {
		struct timeval *val = (struct timeval*)option_value;
		if(SO_RCVTIMEO == option_name) {
			memcpy(&lp->read_timeout,val,sizeof(*val));
		} else if( SO_SNDTIMEO == option_name ) {
			memcpy(&lp->write_timeout,val,sizeof(*val));
		}
	}
	return gSysSetsockopt(fd,level,option_name,option_value,option_len);
}

int fcntl(int fildes, int cmd, ...) {
	if(fildes < 0) {
		return -1;
	}

	va_list arg_list;
	va_start(arg_list,cmd);

	int ret = -1;
	rpchook_t *lp = getByFd(fildes);
	switch(cmd) {
		case F_DUPFD: {
			int param = va_arg(arg_list,int);
			ret = gSysFcntl(fildes,cmd,param);
			break;
		}
		case F_GETFD: {
			ret = gSysFcntl(fildes,cmd);
			break;
		}
		case F_SETFD: {
			int param = va_arg(arg_list,int);
			ret = gSysFcntl(fildes,cmd,param);
			break;
		}
		case F_GETFL: {
			ret = gSysFcntl(fildes,cmd);
			break;
		}
		case F_SETFL: {
			int param = va_arg(arg_list,int);
			int flag = param;
      // set as non block by default
			if(isEnableSysHook() && lp) {
				flag |= O_NONBLOCK;
			}
			ret = gSysFcntl(fildes,cmd,flag);
			if(0 == ret && lp) {
				lp->user_flag = param;
			}
			break;
		}
		case F_GETOWN: {
			ret = gSysFcntl(fildes,cmd);
			break;
		}
		case F_SETOWN: {
			int param = va_arg(arg_list,int);
			ret = gSysFcntl(fildes,cmd,param);
			break;
		}
		case F_GETLK: {
			struct flock *param = va_arg(arg_list,struct flock *);
			ret = gSysFcntl(fildes,cmd,param);
			break;
		}
		case F_SETLK: {
			struct flock *param = va_arg(arg_list,struct flock *);
			ret = gSysFcntl(fildes,cmd,param);
			break;
		}
		case F_SETLKW: {
			struct flock *param = va_arg(arg_list,struct flock *);
			ret = gSysFcntl(fildes,cmd,param);
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
  if (!isEnableSysHook()) {
    return gSysgethostbyname(name);
  }
  return coroutine_gethostbyname(name);
}

void once_init() {
  gSysSocket   = (socket_fun_t)dlsym(RTLD_NEXT,"socket");
  gSysAccept   = (accept_fun_t)dlsym(RTLD_NEXT,"accept");
  gSysConnect = (connect_fun_t)dlsym(RTLD_NEXT,"connect");
  gSysClose   = (close_fun_t)dlsym(RTLD_NEXT,"close");

  gSysSelect     = (select_fun_t)dlsym(RTLD_NEXT,"select");

  gSysSleep     = (sleep_fun_t)dlsym(RTLD_NEXT,"sleep");
  gSysUsleep     = (usleep_fun_t)dlsym(RTLD_NEXT,"usleep");
  gSysNanosleep     = (nanosleep_fun_t)dlsym(RTLD_NEXT,"nanosleep");

  gSysRead     = (read_fun_t)dlsym(RTLD_NEXT,"read");
  gSysWrite   = (write_fun_t)dlsym(RTLD_NEXT,"write");

  sSysSendto   = (sendto_fun_t)dlsym(RTLD_NEXT,"sendto");
  gSysRecvFrom = (recvfrom_fun_t)dlsym(RTLD_NEXT,"recvfrom");

  gSysSend     = (send_fun_t)dlsym(RTLD_NEXT,"send");
  gSysRecv     = (recv_fun_t)dlsym(RTLD_NEXT,"recv");

  gSysPoll     = (poll_fun_t)dlsym(RTLD_NEXT,"poll");

  gSysSetsockopt = (setsockopt_fun_t)dlsym(RTLD_NEXT,"setsockopt");
  gSysFcntl   = (fcntl_fun_t)dlsym(RTLD_NEXT,"fcntl");

  gSysgethostbyname = (gethostbyname_fun_t)dlsym(RTLD_NEXT, "gethostbyname");
}