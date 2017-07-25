#ifndef __TYPEDEF_H__
#define __TYPEDEF_H__

#ifdef __cplusplus
extern "C" {
#endif

// innner struct typedef

struct env_t;
typedef struct env_t env_t;

struct epoll_context_t;
typedef struct epoll_context_t epoll_context_t;

struct epoll_result_t;
typedef struct epoll_result_t epoll_result_t;

struct context_t;
typedef struct context_t context_t;

struct stack_t;
typedef struct stack_t stack_t;

struct timer_item_t;
typedef struct timer_item_t timer_item_t;

struct timer_list_t;
typedef struct timer_list_t timer_list_t;

struct epoll_timer_t;
typedef struct epoll_timer_t epoll_timer_t;

struct task_t;
typedef struct task_t task_t;

struct taskpool_t;
typedef struct taskpool_t taskpool_t;

#ifdef __cplusplus
}
#endif

#endif  // __TYPEDEF_H__
