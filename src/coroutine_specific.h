#ifndef __COROUTINE_SPECIFIC_H__
#define __COROUTINE_SPECIFIC_H__

extern pthread_key_t gCoroutineHostbufKey;

extern void *coroutine_getspecific(pthread_key_t key);
extern int coroutine_setspecific(pthread_key_t key, const void *value);

void coroutine_pthread_key_init();

#endif  // __COROUTINE_SPECIFIC_H__
