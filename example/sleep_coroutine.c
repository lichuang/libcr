#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <pthread.h>
#include <arpa/inet.h> //inet_addr
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/socket.h>
#include "coroutine.h"

// this demo show that: in coroutine,call sleep function should not block the other coroutine in the same thread
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

  coroutine_init_env(&options);

  coroutine_task_attr_t attr;
  attr.enable_sys_hook = 1;
  attr.max_timeout_ms = -1;
  attr.arg = NULL;
  attr.fun = fun1;
  attr.timeout = NULL;

  coroutine_new_task(&attr);

  attr.fun = fun2;
  coroutine_new_task(&attr);
  coroutine_eventloop(NULL);

  return 0;
}
