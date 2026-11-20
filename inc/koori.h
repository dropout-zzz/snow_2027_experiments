#ifndef SNOW_KOORI_H
#define SNOW_KOORI_H

#include <stdint.h>

struct koori_entry_params {
  uint16_t phentsiz;
  uint16_t phnum;
  uintptr_t phbase;
};

__stdcall void koori_entry(struct koori_entry_params *);

#endif /* SNOW_KOORI_H */
