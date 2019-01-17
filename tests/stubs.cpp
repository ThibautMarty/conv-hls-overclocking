extern "C" {

#include <stdint.h>
#include <stdlib.h>

void sds_wait(int unused)
{}

uint64_t sds_clock_counter()
{
  return 0;
}

void *sds_alloc(unsigned int size)
{
  return malloc(size);
}

void *sds_alloc_cacheable(unsigned int size)
{
  return malloc(size);
}

void *sds_alloc_non_cacheable(unsigned int size)
{
  return malloc(size);
}

void sds_free(void *memptr)
{
  free(memptr);
}

}
