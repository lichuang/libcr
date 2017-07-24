#define _GNU_SOURCE
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/syscall.h>
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

typedef int (*socket_pfn_t)(int domain, int type, int protocol);
typedef int (*accept_pfn_t)(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
typedef int (*connect_pfn_t)(int socket, const struct sockaddr *address, socklen_t address_len);
typedef int (*close_pfn_t)(int fd);

typedef ssize_t (*read_pfn_t)(int fildes, void *buf, size_t nbyte);
typedef ssize_t (*write_pfn_t)(int fildes, const void *buf, size_t nbyte);

typedef ssize_t (*sendto_pfn_t)(int socket, const void *message, size_t length,
                     int flags, const struct sockaddr *dest_addr,
                                              socklen_t dest_len);

typedef ssize_t (*recvfrom_pfn_t)(int socket, void *buffer, size_t length,
                     int flags, struct sockaddr *address,
                                              socklen_t *address_len);

typedef size_t (*send_pfn_t)(int socket, const void *buffer, size_t length, int flags);
typedef ssize_t (*recv_pfn_t)(int socket, void *buffer, size_t length, int flags);

typedef int (*poll_pfn_t)(struct pollfd fds[], nfds_t nfds, int timeout);
typedef int (*setsockopt_pfn_t)(int socket, int level, int option_name,
                         const void *option_value, socklen_t option_len);

typedef int (*fcntl_pfn_t)(int fildes, int cmd, ...);
typedef struct tm *(*localtime_r_pfn_t)( const time_t *timep, struct tm *result );

typedef void *(*pthread_getspecific_pfn_t)(pthread_key_t key);
typedef int (*pthread_setspecific_pfn_t)(pthread_key_t key, const void *value);

typedef int (*setenv_pfn_t)(const char *name, const char *value, int overwrite);
typedef int (*unsetenv_pfn_t)(const char *name);
typedef char *(*getenv_pfn_t)(const char *name);
typedef struct hostent* (*gethostbyname_pfn_t)(const char *name);
//typedef res_state (*__res_state_pfn_t)();
typedef int (*__poll_pfn_t)(struct pollfd fds[], nfds_t nfds, int timeout);

// hook system functions
static socket_pfn_t gSysSocket;
static accept_pfn_t gSysAccept;
static connect_pfn_t gSysConnect;
static close_pfn_t gSysClose;

static read_pfn_t gSysRead;
static write_pfn_t gSysWrite;

static sendto_pfn_t sSysSendto;
static recvfrom_pfn_t gSysRecvFrom;

static send_pfn_t gSysSend;
static recv_pfn_t gSysRecv;

static poll_pfn_t gSysPoll;

static setsockopt_pfn_t gSysSetsockopt;
static fcntl_pfn_t gSysFcntl;

static setenv_pfn_t g_sys_setenv_func;
static unsetenv_pfn_t g_sys_unsetenv_func;
static getenv_pfn_t g_sys_getenv_func;
//static __res_state_pfn_t g_sys___res_state_func  = (__res_state_pfn_t)dlsym(RTLD_NEXT,"__res_state");

static gethostbyname_pfn_t g_sys_gethostbyname_func;

static __poll_pfn_t g_sys___poll_func;

typedef struct rpchook_t
{
	int user_flag;
	struct sockaddr_in dest; //maybe sockaddr_un;
	int domain; //AF_LOCAL , AF_INET

	struct timeval read_timeout;
	struct timeval write_timeout;
}rpchook_t;
static rpchook_t *gSocketFd[ 102400 ] = { 0 };

static inline rpchook_t* get_by_fd(int fd) {
	if( fd > -1 && fd < (int)sizeof(gSocketFd) / (int)sizeof(gSocketFd[0]) ) {
		return gSocketFd[fd];
	}
	return NULL;
}

static inline rpchook_t * alloc_by_fd(int fd) {
	if( fd > -1 && fd < (int)sizeof(gSocketFd) / (int)sizeof(gSocketFd[0]) ) {
		rpchook_t *lp = (rpchook_t*)calloc(1,sizeof(rpchook_t));
		lp->read_timeout.tv_sec = 1;
		lp->write_timeout.tv_sec = 1;
		gSocketFd[ fd ] = lp;
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
	if(!is_enable_sys_hook()) {
		return gSysSocket(domain,type,protocol);
	}
	int fd = gSysSocket(domain,type,protocol);
	if( fd < 0 ) {
		return fd;
	}

	rpchook_t *lp = alloc_by_fd( fd );
	lp->domain = domain;
	
  // set socket fd as non block by default
	fcntl(fd, F_SETFL, gSysFcntl(fd, F_GETFL,0) | O_NONBLOCK);

	return fd;
}

int accept(int listen_fd, struct sockaddr *addr, socklen_t *len) {
	int fd = gSysAccept(listen_fd,addr,len);

  do {
    if(fd < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        struct pollfd pf = {0};
        pf.fd = listen_fd;
        pf.events = (POLLIN|POLLERR|POLLHUP);
        coroutine_poll(get_epoll_context(),&pf,1,1000);
	      fd = gSysAccept(listen_fd,addr,len);
        continue;
      }
      return fd;
    }
  } while (fd < 0);

	rpchook_t *lp = alloc_by_fd(fd);

  int current_flags = gSysFcntl(fd ,F_GETFL, 0);
  int flag = current_flags;
  flag |= O_NONBLOCK;

  // set socket fd as non block by default
  int ret = gSysFcntl(fd ,F_SETFL, flag);
  if( 0 == ret && lp) {
    lp->user_flag = current_flags;
  }

	return fd;
}

int connect(int fd, const struct sockaddr *address, socklen_t address_len) {
	if(!is_enable_sys_hook()) {
		return gSysConnect(fd,address,address_len);
	}

	//1.sys call
	int ret = gSysConnect(fd,address,address_len);

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
	int pollret = 0;
	struct pollfd pf = { 0 };

	for(int i=0;i<3;i++) { //25s * 3 = 75s 
		memset( &pf,0,sizeof(pf) );
		pf.fd = fd;
		pf.events = (POLLOUT | POLLERR | POLLHUP);

		pollret = poll(&pf,1,25000);

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
	if(!is_enable_sys_hook()) {
		return gSysClose(fd);
	}

	free_by_fd(fd);
	return gSysClose(fd);
}

ssize_t read(int fd, void *buf, size_t nbyte) {
	if(!is_enable_sys_hook()) {
		return gSysRead(fd,buf,nbyte);
	}

	rpchook_t *lp = get_by_fd(fd);

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

ssize_t write(int fd, const void *buf, size_t nbyte) {
	if(!is_enable_sys_hook()) {
		return gSysWrite(fd,buf,nbyte);
	}

	rpchook_t *lp = get_by_fd(fd);

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
	if(!is_enable_sys_hook()) {
		return gSysSend(socket,buffer,length,flags);
	}

	rpchook_t *lp = get_by_fd(socket);

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
	if(!is_enable_sys_hook()) {
		return gSysRecv(socket,buffer,length,flags);
	}
	rpchook_t *lp = get_by_fd(socket);

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

void once_init() {
  gSysSocket   = (socket_pfn_t)dlsym(RTLD_NEXT,"socket");
  gSysAccept   = (accept_pfn_t)dlsym(RTLD_NEXT,"accept");
  gSysConnect = (connect_pfn_t)dlsym(RTLD_NEXT,"connect");
  gSysClose   = (close_pfn_t)dlsym(RTLD_NEXT,"close");

  gSysRead     = (read_pfn_t)dlsym(RTLD_NEXT,"read");
  gSysWrite   = (write_pfn_t)dlsym(RTLD_NEXT,"write");

  sSysSendto   = (sendto_pfn_t)dlsym(RTLD_NEXT,"sendto");
  gSysRecvFrom = (recvfrom_pfn_t)dlsym(RTLD_NEXT,"recvfrom");

  gSysSend     = (send_pfn_t)dlsym(RTLD_NEXT,"send");
  gSysRecv     = (recv_pfn_t)dlsym(RTLD_NEXT,"recv");

  gSysPoll     = (poll_pfn_t)dlsym(RTLD_NEXT,"poll");

  gSysSetsockopt = (setsockopt_pfn_t)dlsym(RTLD_NEXT,"setsockopt");
  gSysFcntl   = (fcntl_pfn_t)dlsym(RTLD_NEXT,"fcntl");

  g_sys_setenv_func   = (setenv_pfn_t)dlsym(RTLD_NEXT,"setenv");
  g_sys_unsetenv_func = (unsetenv_pfn_t)dlsym(RTLD_NEXT,"unsetenv");
  g_sys_getenv_func   =  (getenv_pfn_t)dlsym(RTLD_NEXT,"getenv");
  //static __res_state_pfn_t g_sys___res_state_func  = (__res_state_pfn_t)dlsym(RTLD_NEXT,"__res_state");

  g_sys_gethostbyname_func = (gethostbyname_pfn_t)dlsym(RTLD_NEXT, "gethostbyname");

  g_sys___poll_func = (__poll_pfn_t)dlsym(RTLD_NEXT, "__poll");
}
