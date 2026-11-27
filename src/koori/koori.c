#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <elf.h>
#include <windows.h>

#ifndef _UCRT
#error snowapp assumes UCRT
#endif

#include "koori.h"
#include "_snowin.h"

#define do_early_abort(fmt, ...) \
  early_abort_cb("koori early abort: " fmt, ##__VA_ARGS__)

extern Elf64_Dyn _DYNAMIC[];

extern SNOWIN_LIBRARY __snowinlibs[];
extern char __snowinlibs_end;

extern SNOWIN_FUNCTION __snowinfuncs[];
extern char __snowinfuncs_end;

__stdcall void koori_entry(struct koori_entry_params *args) {
  Elf64_Addr relro_start;
  size_t relro_siz = 0;
  uintptr_t base;

  if (args->phentsiz < sizeof(Elf64_Phdr))
    args->do_early_abort("seg tbl item small\n");

  for (int i = 0; i < args->phnum; i++) {
    size_t phdr_off = i * args->phentsiz;
    Elf64_Phdr *phdr = (Elf64_Phdr *)(args->phbase + phdr_off);

    switch (phdr->p_type) {
      case PT_GNU_RELRO:
        relro_start = phdr->p_vaddr;
        relro_siz = phdr->p_memsz;
        break;

      case PT_DYNAMIC:
        base = (uintptr_t)_DYNAMIC - phdr->p_vaddr;
        break;
    }
  }

  uintptr_t relas_base;
  size_t rela_tbl_siz = 0;
  size_t rela_ent_siz = sizeof(Elf64_Rela);

  for (Elf64_Dyn *dyn = _DYNAMIC; dyn->d_tag != DT_NULL; dyn++) {
    switch (dyn->d_tag) {
      case DT_RELA:
        relas_base = dyn->d_un.d_ptr + base;
        break;

      case DT_RELASZ:
        rela_tbl_siz = dyn->d_un.d_val;
        break;

      case DT_RELAENT:
        if (sizeof(Elf64_Rela) > dyn->d_un.d_val)
          args->do_early_abort("rela tbl item small\n");

        rela_ent_siz = dyn->d_un.d_val;
        break;
    }
  }

  size_t rela_count = rela_tbl_siz / rela_ent_siz;

  for (size_t i = 0; i < rela_count; i++) {
    size_t rela_off = i * rela_ent_siz;
    Elf64_Rela *rela = (Elf64_Rela *)(relas_base + rela_off);
    unsigned int reloc_typ = ELF64_R_TYPE(rela->r_info);

    switch (reloc_typ) {
#ifdef __x86_64__
      case R_X86_64_RELATIVE:
        *(uintptr_t *)(base + rela->r_offset) = base + rela->r_addend;
        break;
#endif
      default:
        args->do_early_abort("fixme reloc: %u\n", reloc_typ);
    }
  }

  for (PSNOWIN_LIBRARY library = __snowinlibs; (char *)library != &__snowinlibs_end; library++) {
    library->Handle = args->load_lib_cb(library->Name);
  }

  for (PSNOWIN_FUNCTION function = __snowinfuncs; (char *)function != &__snowinfuncs_end; function++) {
    function->Address = args->get_fn_addr_cb(function->Library->Handle, function->Name);
  }

  DWORD dummy;

  if (relro_siz > 0
      && VirtualProtect((void *)(base + relro_start),
                        relro_siz,
                        PAGE_READONLY,
                        &dummy) == FALSE) {
    fprintf(stderr, "err=%lu applying koori RELRO\n", GetLastError());
    exit(1);
  }
}
