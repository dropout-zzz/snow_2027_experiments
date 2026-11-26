#ifndef SNOW_KOORI_H
#define SNOW_KOORI_H

#include <stdint.h>

__attribute__((noreturn,format(printf,1,2)))
__cdecl void koori_early_abort(const char *, ...);

__stdcall void *snowin_open_dll(const char *);
__stdcall void *snowin_get_func(void *, const char *);

struct koori_entry_params {
  uint16_t phentsiz;
  uint16_t phnum;
  uintptr_t phbase;

  typeof(&koori_early_abort) early_abort_cb;

  typeof(&snowin_open_dll) load_lib_cb;
  typeof(&snowin_get_func) get_fn_addr_cb;
};

__stdcall void koori_entry(struct koori_entry_params *);

#endif /* SNOW_KOORI_H */
