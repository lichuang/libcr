#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <pthread.h>
#include <arpa/inet.h> //inet_addr
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/socket.h>
#include "coroutine.h"

int count = 0;

void *fun1(void *arg) {
  while (1) {
    printf("in fun1\n");
    sleep(1);
  }

  return NULL;
}

void *fun2(void *arg) {
  while (1) {
    printf("in fun2\n");
    sleep(5);
  }
  return NULL;
}

int main() {
  coroutine_options_t options;
  options.stack_size = 8 * 1024;
  options.enable_sys_hook = 1;

  coroutine_init_env(&options);
  coroutine_new_task(fun1, NULL);
  coroutine_new_task(fun2, NULL);
  coroutine_eventloop();

  return 0;
}
