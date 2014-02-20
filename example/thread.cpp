#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <arpa/inet.h> //inet_addr
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/socket.h>

extern void Run();

void *echo(void *arg) {
  int fd = *((int*)arg);
  char buf[1024];

  printf("in echo\n");
  while (true) {
    ssize_t n = recv(fd, buf, sizeof(buf), 0);
    if (n <= 0) {
      break;
    }
    buf[n] = '\0';

    n = send(fd, buf, n, 0);
    if (n <= 0) {
      break;
    }
  }

  return NULL;
}

void *listener(void *arg) {
  struct sockaddr_in server;
  int socket_desc = socket(AF_INET , SOCK_STREAM , 0);

  server.sin_family = AF_INET;
  server.sin_addr.s_addr = INADDR_ANY;
  server.sin_port = htons(8888);

  //fcntl(socket_desc, F_SETFL, fcntl(socket_desc, F_GETFL) | O_NONBLOCK);
  bind(socket_desc,(struct sockaddr *)&server , sizeof(server));
  int optval = 1;
  setsockopt(socket_desc, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);
  listen(socket_desc, 200);

  while (true) {
    int fd = accept(socket_desc, NULL, NULL);

    if (fd != -1) {
      fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);

      pthread_t id;
      pthread_create(&id, NULL, echo, &fd);
    }
  }

  return NULL;
}

int main() {
  pthread_t id;
  pthread_create(&id, NULL, listener, NULL);

  Run();

  return 0;
}
