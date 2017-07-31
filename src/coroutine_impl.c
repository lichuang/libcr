#include <pthread.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <poll.h>
#include "assert.h"
#include "coroutine.h"
#include "coroutine_impl.h"
#include "coroutine_specific.h"
#include "context.h"
#include "misc.h"

static const int kMaxCallStack = 128;
static const int kMinStackSize = 128 * 1024;
static const int kMaxStackSize = 8 * 1024 * 1024;
static const int kMaxTimeoutMs = 40 * 1000;
static const int kDefaultIoTimeoutMs = 1000;
static const int kDefaultTaskPerThread = 1000;

coroutine_options_t gOptions;

static env_t* gEnv[204800] = { NULL };

static inline pid_t get_pid() {
  char **p = (char**)pthread_self();
  return p ? *(pid_t*)(p + 18) : getpid();
}

env_t* get_curr_thread_env() {
  return gEnv[get_pid()];
}

static inline stack_t* alloc_stack(int size) {
  stack_t *stack = (stack_t*)malloc(sizeof(stack_t));
  stack->size = size;
  stack->start = (char*)malloc(sizeof(char) * size);
  stack->end = stack->start + size;

  return stack;
}

static coroutine_t* create_env(env_t *env, coroutine_fun_t func, void *arg) {
  int stack_size = gOptions.stack_size;

  coroutine_t *co = (coroutine_t*)malloc(sizeof(coroutine_t));
  memset(co, 0, sizeof(coroutine_t));
  co->env = env;
  co->fun = func;
  co->arg = arg;
  co->state = INIT;

  stack_t *stack = alloc_stack(stack_size);
  co->stack = stack;
  co->context.sp = stack->start;
  co->context.size = stack_size;

  co->save_size = 0;
  co->save_buffer = NULL;

  return co;
}

env_t* init_curr_thread_env() {
  pid_t pid = 0;
  env_t *env = NULL;

  pid = get_pid();
  env = (env_t*)calloc(1, sizeof(env_t));
  env->callstack = (coroutine_t**)calloc(kMaxCallStack, sizeof(coroutine_t*));
  env->callstacksize = 0;
  gEnv[pid] = env;

  coroutine_t *co = create_env(env, NULL, NULL);
  co->main = 1;

  context_init(&co->context);
  env->callstack[env->callstacksize++] = co;
  env->epoll = alloc_epoll(10240);

  return env;
}

void do_init_curr_thread_env() {
  env_t *env = init_curr_thread_env();
  env->pool = create_thread_taskpool(env, gOptions.task_per_thread);
}

coroutine_t* coroutine_new(coroutine_fun_t fun, void *arg) {
  env_t *env = get_curr_thread_env();
  if (env == NULL) {
    do_init_curr_thread_env();
    env = get_curr_thread_env();
  }

  return create_env(env, fun, arg);
}

void coroutine_new_task(coroutine_task_attr_t *attr) {
  env_t *env = get_curr_thread_env();
  if (env == NULL) {
    do_init_curr_thread_env();
    env = get_curr_thread_env();
  }

  new_task(env->pool, attr);
}

void coroutine_free(coroutine_t *co) {
  free(co);
}

void coroutine_swap(coroutine_t* curr, coroutine_t* pending) {
 	env_t* env = get_curr_thread_env();

	//get curr stack sp
	char c;
	curr->stack_sp = &c;

  env->pending = NULL;
	env->occupy = NULL;

	//swap context
	context_swap(&(curr->context), &(pending->context));

	//stack buffer may be overwrite, so get again;
	env_t* curr_env = get_curr_thread_env();
	coroutine_t* update_occupy =  curr_env->occupy;
	coroutine_t* update_pending = curr_env->pending;
	
	if (update_occupy && update_pending && update_occupy != update_pending) {
		//resume stack buffer
		if (update_pending->save_buffer && update_pending->save_size > 0) {
			memcpy(update_pending->stack_sp, update_pending->save_buffer, update_pending->save_size);
		}
	}
}

static void yield_env(env_t *env) {
  int size = env->callstacksize;
  coroutine_t *last = env->callstack[size - 2];
  coroutine_t *curr = env->callstack[size - 1];
  env->callstacksize--;

  coroutine_swap(curr, last);
}

static void* coroutine_main(void *arg, void *s1) {
  (void)s1;
  coroutine_t *co = (coroutine_t*)arg;
  if (co->fun) {
    co->fun(co->arg);
  }
  co->state = STOPPED;

  env_t *env = co->env;
  //coroutine_free(co);

  yield_env(env);

  return NULL;
}

void coroutine_resume(coroutine_t *co) {
  env_t *env = co->env;
  coroutine_t *curr = env->callstack[env->callstacksize - 1];
  if (co->state != RUNNING) {
    context_make(&co->context, coroutine_main, co, NULL);
    co->state = RUNNING;
  }

  env->callstack[env->callstacksize++] = co;
  coroutine_swap(curr, co);
}

void coroutine_yield(coroutine_t *co) {
  yield_env(co->env);
}

void coroutine_yield_context() {
  yield_env(get_curr_thread_env());
}

static coroutine_t *curr_coroutine(env_t *env) {
  return env->callstack[env->callstacksize - 1];
}

coroutine_t* coroutine_self() {
  env_t *env = get_curr_thread_env();
  if (env == NULL) {
    return NULL;
  }

  return curr_coroutine(env);
}

coroutine_t *get_curr_coroutine(env_t *env) {
  return env->callstack[env->callstacksize - 1];
}

struct poll_context_t;

typedef struct poll_item_t {
  struct pollfd *self;
  struct poll_context_t *poll;
  timer_item_t time;

  struct epoll_event event;
} poll_item_t;

typedef struct poll_context_t {
  struct pollfd *fds;
  nfds_t nfds; // typedef unsigned long int nfds_t;

  timer_item_t time;
  poll_item_t *items;

  char allEventDetatch;

  int raise_cnt;
} poll_context_t;

/*
 *   EPOLLPRI 		POLLPRI    // There is urgent data to read.  
 *   EPOLLMSG 		POLLMSG
 *
 *   				POLLREMOVE
 *   				POLLRDHUP
 *   				POLLNVAL
 *
 * */
static inline uint32_t PollEvent2Epoll(short events) {
	uint32_t e = 0;	
	if( events & POLLIN ) 	e |= EPOLLIN;
	if( events & POLLOUT )  e |= EPOLLOUT;
	if( events & POLLHUP ) 	e |= EPOLLHUP;
	if( events & POLLERR )	e |= EPOLLERR;
	if( events & POLLIN ) e |= EPOLLIN;
	if( events & POLLOUT ) e |= EPOLLOUT;
	return e;
}

static inline short EpollEvent2Poll(uint32_t events) {
	short e = 0;	
	if( events & EPOLLIN ) 	e |= POLLIN;
	if( events & EPOLLOUT ) e |= POLLOUT;
	if( events & EPOLLHUP ) e |= POLLHUP;
	if( events & EPOLLERR ) e |= POLLERR;
	if( events & EPOLLIN ) e |= POLLIN;
	if( events & EPOLLOUT ) e |= POLLOUT;
	return e;
}

static void processPollEvent(timer_item_t *item) {
  coroutine_resume(item->coroutine);
}

static void preparePollEvent(timer_item_t *item,struct epoll_event *ev,timer_list_t *active) {
  poll_item_t *poll_item = (poll_item_t*)item->arg;
	poll_item->self->revents = EpollEvent2Poll(ev->events);

	poll_context_t *poll = poll_item->poll;
	poll->raise_cnt++;

	if (!poll->allEventDetatch) {
		poll->allEventDetatch = 1;
		remove_from_link(&poll->time);
		add_tail(active, &poll->time);
	}
}

unsigned long long get_now() {
  env_t *env = get_curr_thread_env();
  return get_epoll_now(env->epoll);
}

// return:
//  > 0: timeout ms
//  < 0: no timeout limit
//  = 0: already timeout
static int fix_coroutine_timeout(coroutine_t *co, int timeout) {
  task_t *task = co->task;
  // attr no timeout limit
  if (task->attr.max_timeout_ms == -1) {
    return timeout;
  }
  if (task->timeout) {
    // already timeout
    return 0;
  }
  unsigned long long now = get_now();
  int diff = now - task->last;
  ASSERT(diff >= 0);
  task->leftmsec -= diff;
  task->last = now;
  if (task->leftmsec <= 0) {
    task->timeout = 1;
    return 0;
  }

  if (timeout == -1) {
    timeout = task->leftmsec;
  } else {
    timeout = timeout > task->leftmsec ? task->leftmsec : timeout;
  }

  return timeout;
}

int poll_inner(epoll_context_t *ctx, struct pollfd fds[], nfds_t nfds, int timeout, poll_fun_t pollfunc) {
	coroutine_t* self = coroutine_self();
  timeout = fix_coroutine_timeout(self, timeout);
  if (timeout == 0) {
    // timeout
    return 0;
  }

	if(timeout == -1 || timeout > kMaxTimeoutMs) {
		timeout = kMaxTimeoutMs;
	}
	int epfd = ctx->fd;

	//1.struct change
	poll_context_t arg;
	memset(&arg,0,sizeof(arg));

	arg.fds = (struct pollfd*)calloc(nfds, sizeof(struct pollfd));
	arg.nfds = nfds;

	poll_item_t items[2];
	if( nfds < sizeof(items) / sizeof(items[0])) {
		arg.items = items;
	}	else {
		arg.items = (poll_item_t*)malloc(nfds * sizeof(poll_item_t));
	}
	memset(arg.items, 0, nfds * sizeof(poll_item_t));

	arg.time.process = processPollEvent;
	arg.time.coroutine = self;
  arg.time.arg = &arg;
	
	//2. add epoll
	for(nfds_t i = 0; i < nfds; i++) {
		arg.items[i].self = arg.fds + i;
		arg.items[i].poll = &arg;

		arg.items[i].time.prepare = preparePollEvent;
		arg.items[i].time.coroutine = self;
		arg.items[i].time.arg = &(arg.items[i]);
		struct epoll_event *ev = &(arg.items[i].event);

		if(fds[i].fd > -1) {
			ev->data.ptr = &(arg.items[i].time);
			ev->events = PollEvent2Epoll(fds[i].events);

			int ret = do_epoll_ctl(epfd,EPOLL_CTL_ADD, fds[i].fd, ev);
			if (ret < 0 && errno == EPERM && nfds == 1 && pollfunc != NULL) {
				if(arg.items != items) {
					free(arg.items);
					arg.items = NULL;
				}
				free(arg.fds);
				return pollfunc(fds, nfds, timeout);
			}
		}
		//if fail,the timeout would work
	}

	//3.add timeout
	unsigned long long now = GetTickMS();
	arg.time.expire = now + timeout;
	int ret = add_timeout(ctx->timer, &arg.time, now);
	if(ret != 0) {
		errno = EINVAL;

		if(arg.items != items) {
			free(arg.items);
			arg.items = NULL;
		}
		free(arg.fds);

		return -1;
	}

	yield_env(get_curr_thread_env());

	remove_from_link(&arg.time);

	for(nfds_t i = 0;i < nfds;i++) {
		int fd = fds[i].fd;
		if(fd > -1) {
			do_epoll_ctl(epfd,EPOLL_CTL_DEL,fd,&arg.items[i].event);
		}
		fds[i].revents = arg.fds[i].revents;
	}

	int raise_cnt = arg.raise_cnt;
  if(arg.items != items) {
    free(arg.items);
    arg.items = NULL;
  }
  free(arg.fds);

	return raise_cnt;
}

epoll_context_t *get_epoll_context() {
  env_t *env = get_curr_thread_env();

  if (env == NULL) {
    do_init_curr_thread_env();
    env = get_curr_thread_env();
  }

  return env->epoll;
}

coroutine_t *get_curr_thread_coroutine( ) {
  env_t *env = get_curr_thread_env();
  if(!env) {
    return 0;
  }
  return get_curr_coroutine(env);
}

void *coroutine_getspecific(pthread_key_t key) {
  coroutine_t *co = get_curr_thread_coroutine();
  if(!co || co->main) {
    return pthread_getspecific(key);
  }
  return co->spec[key].value;
}

int coroutine_setspecific(pthread_key_t key, const void *value) {
  coroutine_t *co = get_curr_thread_coroutine();
  if(!co || co->main) {
    return pthread_setspecific(key,value);
  }
  co->spec[key].value = (void*)value;
  return 0;
}

int	coroutine_poll(epoll_context_t *ctx,struct pollfd fds[], nfds_t nfds, int timeout_ms) {
	return poll_inner(ctx, fds, nfds, timeout_ms, NULL);
}

static pthread_once_t once = PTHREAD_ONCE_INIT;
extern void once_init();

void coroutine_init_env(const coroutine_options_t *options) {
  pthread_once(&once, once_init);
  coroutine_pthread_key_init();

  memcpy(&gOptions, options, sizeof(coroutine_options_t));
  int stack_size = gOptions.stack_size;

  if (stack_size < kMinStackSize) {
    stack_size = kMinStackSize;
  } else if (stack_size > kMaxStackSize) {
    stack_size = kMaxStackSize;
  }

  if (stack_size & 0XFFF) {
    stack_size &= ~0XFFF;
    stack_size += 0x1000;
  }
  gOptions.stack_size = stack_size;

  if (gOptions.task_per_thread == 0) {
    gOptions.task_per_thread = kDefaultTaskPerThread;
  }
}
