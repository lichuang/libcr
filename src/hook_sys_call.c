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

