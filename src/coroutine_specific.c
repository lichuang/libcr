#include <pthread.h>
#include "coroutine_specific.h"

pthread_key_t gCoroutineHostbufKey;

void coroutine_pthread_key_init() {
  pthread_key_create(&gCoroutineHostbufKey, NULL);  
}

