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

void *echo(void *arg) {
  int fd = *((int*)arg);
  char buf[1024];

  //printf("in echo\n");
  while (1) {
    ssize_t n = recv(fd, buf, sizeof(buf), 0);
    if (n <= 0) {
      close(fd);
      break;
    }
    buf[n] = '\0';
    //printf("echo recv: %s, n: %ld\n", buf, n);

    n = send(fd, buf, n, 0);
    if (n <= 0) {
      close(fd);
      break;
    }
  }

  printf("end:%d\n", ++count);
  return NULL;
}

void *listener(void *arg) {
  printf("in listener\n");
  struct sockaddr_in server;
  int socket_desc = socket(AF_INET , SOCK_STREAM , 0);

  server.sin_family = AF_INET;
  server.sin_addr.s_addr = INADDR_ANY;
  server.sin_port = htons(28888);

  fcntl(socket_desc, F_SETFL, fcntl(socket_desc, F_GETFL) | O_NONBLOCK);
  bind(socket_desc,(struct sockaddr *)&server , sizeof(server));
  int optval = 1;
  setsockopt(socket_desc, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);
  listen(socket_desc, 20000);

  while (1) {
    int fd = accept(socket_desc, NULL, NULL);

    printf("accept %d, count:%d\n", fd, count);
    if (fd > 0) {
      coroutine_new_task(echo, &fd);
    }
  }

  return NULL;
}

int main() {
  coroutine_options_t options;
  options.stack_size = 8 * 1024;
  options.enable_sys_hook = 1;

  coroutine_init_env(&options);
  coroutine_new_task(listener, NULL);
  coroutine_eventloop();

  return 0;
}
