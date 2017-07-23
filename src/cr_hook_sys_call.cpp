#include <pthread.h>

static pthread_once_t once = PTHREAD_ONCE_INIT;

typedef int (*socket_pfn_t)(int domain, int type, int protocol);
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
typedef hostent* (*gethostbyname_pfn_t)(const char *name);
typedef res_state (*__res_state_pfn_t)();
typedef int (*__poll_pfn_t)(struct pollfd fds[], nfds_t nfds, int timeout);

// hook system functions
static socket_pfn_t gSysSocket   = (socket_pfn_t)dlsym(RTLD_NEXT,"socket");
static connect_pfn_t gSysConnect = (connect_pfn_t)dlsym(RTLD_NEXT,"connect");
static close_pfn_t gSysClose   = (close_pfn_t)dlsym(RTLD_NEXT,"close");

static read_pfn_t gSysRead     = (read_pfn_t)dlsym(RTLD_NEXT,"read");
static write_pfn_t gSysWrite   = (write_pfn_t)dlsym(RTLD_NEXT,"write");

static sendto_pfn_t sSysSendto   = (sendto_pfn_t)dlsym(RTLD_NEXT,"sendto");
static recvfrom_pfn_t gSysRecvFrom = (recvfrom_pfn_t)dlsym(RTLD_NEXT,"recvfrom");

static send_pfn_t gSysSend     = (send_pfn_t)dlsym(RTLD_NEXT,"send");
static recv_pfn_t gSysRecv     = (recv_pfn_t)dlsym(RTLD_NEXT,"recv");

static poll_pfn_t gSysPoll     = (poll_pfn_t)dlsym(RTLD_NEXT,"poll");

static setsockopt_pfn_t gSysSetsockopt = (setsockopt_pfn_t)dlsym(RTLD_NEXT,"setsockopt");
static fcntl_pfn_t gSysFcntl   = (fcntl_pfn_t)dlsym(RTLD_NEXT,"fcntl");

static setenv_pfn_t g_sys_setenv_func   = (setenv_pfn_t)dlsym(RTLD_NEXT,"setenv");
static unsetenv_pfn_t g_sys_unsetenv_func = (unsetenv_pfn_t)dlsym(RTLD_NEXT,"unsetenv");
static getenv_pfn_t g_sys_getenv_func   =  (getenv_pfn_t)dlsym(RTLD_NEXT,"getenv");
static __res_state_pfn_t g_sys___res_state_func  = (__res_state_pfn_t)dlsym(RTLD_NEXT,"__res_state");

static gethostbyname_pfn_t g_sys_gethostbyname_func = (gethostbyname_pfn_t)dlsym(RTLD_NEXT, "gethostbyname");

static __poll_pfn_t g_sys___poll_func = (__poll_pfn_t)dlsym(RTLD_NEXT, "__poll");

