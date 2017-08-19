#ifndef __CONTEXT_H__
#define __CONTEXT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "coroutine.h"
#include "typedef.h"
#ifdef USE_UCONTEXT
#include <ucontext.h>  
#endif

struct context_t {
#ifdef USE_UCONTEXT
  ucontext_t ut;
#elif defined(__i386__)
  void *regs[ 8 ];
#else
  void *regs[ 14 ];
#endif
  size_t size;
  char *sp;
};

typedef void (*context_fun_t)(void* s);

int context_init(context_t *context);
int context_make(context_t *ctx, context_fun_t fun, const void *s);

#ifdef USE_UCONTEXT
void ucontext_swap(context_t* curr, context_t* pending);
#else
void context_swap(context_t* curr, context_t* pending);
#endif

#ifdef __cplusplus
}
#endif

#endif  // __CONTEXT_H__
