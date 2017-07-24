#include <stdio.h>
#include <sys/time.h>
#include "misc.h"

unsigned long long GetTickMS() {
#if defined( __LIBCO_RDTSCP__)
  static uint32_t khz = getCpuKhz();
  return counter() / khz;
#else
  struct timeval now = { 0 };
  gettimeofday( &now,NULL );
  unsigned long long u = now.tv_sec;
  u *= 1000;
  u += now.tv_usec / 1000;
  return u;
#endif
}
