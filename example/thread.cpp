#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

void *func(void *arg) {
  int i = (int)arg;
  printf("thread %d\n", i);

  return NULL;
}

int main() {
  int i;
  for (i = 0; i < 5; ++i) {
    pthread_t id;
    pthread_create(&id, NULL, func, (void*)i);
  }

  sleep(1);

  return 0;
}
