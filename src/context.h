#ifndef __CONTEXT_H__
#define __CONTEXT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "coroutine.h"
#include "typedef.h"

struct context_t {
#if defined(__i386__)
  void *regs[ 8 ];
#else
  void *regs[ 14 ];
#endif
  size_t size;
  char *sp;
};

typedef int (*context_fun_t)(void*, void*);

int context_init(context_t *context);
int context_make(context_t *ctx, context_fun_t fun, const void *s, const void *s1);

void context_swap(context_t* curr, context_t* pending);

#ifdef __cplusplus
}
#endif

#endif  // __CONTEXT_H__
