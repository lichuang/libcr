#include <string.h>
#include "idmap.h"

/* page size = 2^12 = 4K */
#define PAGE_SHIFT    12
#define PAGE_SIZE    (1UL << PAGE_SHIFT)

#define BITS_PER_BYTE        8
#define BITS_PER_PAGE        (PAGE_SIZE * BITS_PER_BYTE)
#define BITS_PER_PAGE_MASK    (BITS_PER_PAGE - 1)

IdMap::IdMap()
  : nr_free_(ID_MAX_DEFAULT),
    last_id_(-1) {
  memset(page_, 0, ID_MAX_DEFAULT);
}

IdMap::~IdMap() {
}

static int
test_and_set_bit(int offset, void *addr) {
  unsigned long mask = 1UL << (offset & (sizeof(unsigned long) * BITS_PER_BYTE - 1));    
  unsigned long *p = ((unsigned long*)addr) + (offset >> (sizeof(unsigned long) + 1));
  unsigned long old = *p;    

  *p = old | mask;    

  return (old & mask) != 0;
}

static void
clear_bit(int offset, void *addr) {
  unsigned long mask = 1UL << (offset & (sizeof(unsigned long) * BITS_PER_BYTE - 1));    
  unsigned long *p = ((unsigned long*)addr) + (offset >> (sizeof(unsigned long) + 1));
  unsigned long old = *p;    

  *p = old & ~mask;    
}

static int
find_next_zero_bit(void *addr, int size, int offset) {
  unsigned long *p;
  unsigned long mask;

  while (offset < size) {
    p = ((unsigned long*)addr) + (offset >> (sizeof(unsigned long) + 1));
    mask = 1UL << (offset & (sizeof(unsigned long) * BITS_PER_BYTE - 1));    

    if ((~(*p) & mask)) {
      break;
    }

    ++offset;
  }

  return offset;
}

int
IdMap::Allocate() {
  int id = last_id_ + 1;
  int offset = id & BITS_PER_PAGE_MASK;

  if (!nr_free_) {
    return -1;
  }

  offset = find_next_zero_bit(&page_, BITS_PER_PAGE, offset);
  if (BITS_PER_PAGE != offset &&
    !test_and_set_bit(offset, &page_)) {
    --nr_free_;
    last_id_ = offset;
    return offset;
  }

  return -1;
}

void
IdMap::Free(int id) {
  int offset = id & BITS_PER_PAGE_MASK;

  nr_free_++;
  clear_bit(offset, &page_);
}
