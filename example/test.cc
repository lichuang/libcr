#include <stdio.h>
#include "coroutine.h"

struct Info {
  int num;
  int id;
};

void *Func(void *arg) {
  Info *info = (Info*)arg;
  int i = info->num;

  while (i > 0) {
    gSched.Yield();
    printf("i:%d\n", i);
    --i;
  }

  return NULL;
}

int main() {
  Info info;

  info.num = 10;
  info.id  = gSched.Spawn(&info, Func);

  gSched.Run();

  return 0;
}
