#ifndef __CONTEXT_H__
#define __CONTEXT_H__

struct context_t {
#if defined(__i386__)
  void *regs[ 8 ];
#else
  void *regs[ 14 ];
#endif
  size_t size;
  char *sp;
};

typedef void* (*context_fun_t)(void*, void*);

int context_init(context_t *context);
int context_make(context_t *ctx, context_fun_t fun, const void *s, const void *s1);

void context_swap(coroutine_t* curr, coroutine_t* pending);

#endif  // __CONTEXT_H__
