#ifndef __COMMON_H__
#define __COMMON_H__

typedef void* (*cfunc)(void*);

// stack size
static const int kProtectStackSize = 10 * 1024;
static const int kDefaulaStackSize = 100 * 1024;

// coroutine num
static const int kCoroNum = 256;

#endif  /* __COMMON_H__ */
