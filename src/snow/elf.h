#ifndef SNOW_ELF_H
#define SNOW_ELF_H

#include <stdint.h>

#define EI_NIDENT 16

#define EI_CLASS 4
#define ELFCLASS64 0x02

#define EI_DATA 5
#define ELFDATA2LSB 0x01

#define ET_DYN 3

#define EM_X86_64 0x3e

#define PN_XNUM 0xffff

#define PT_LOAD 1
#define PT_INTERP 3

#define PF_R 4
#define PF_W 2
#define PF_X 1

typedef uint64_t Elf64_Addr;
typedef uint64_t Elf64_Off;

typedef struct {
  unsigned char e_ident[EI_NIDENT];
  uint16_t      e_type;
  uint16_t      e_machine;
  uint32_t      e_version;
  Elf64_Addr    e_entry;
  Elf64_Off     e_phoff;
  Elf64_Off     e_shoff;
  uint32_t      e_flags;
  uint16_t      e_ehsize;
  uint16_t      e_phentsize;
  uint16_t      e_phnum;
  uint16_t      e_shentsize;
  uint16_t      e_shnum;
  uint16_t      e_shstrndx;
} Elf64_Ehdr;

typedef struct {
  uint32_t   p_type;
  uint32_t   p_flags;
  Elf64_Off  p_offset;
  Elf64_Addr p_vaddr;
  Elf64_Addr p_paddr;
  uint64_t   p_filesz;
  uint64_t   p_memsz;
  uint64_t   p_align;
} Elf64_Phdr;

#endif /* SNOW_ELF_H */
