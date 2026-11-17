#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "elf.h"

#define iadd __builtin_add_overflow

#define ROUND_DOWN(x,y) ((x) - (x) % (y))

#define ROUND_UP_CHKED(x,y,out)                  \
  iadd((x),                                      \
       ((x) % (y) == 0) ? 0 : ((y) - (x) % (y)), \
       (out))

#define eprintf(...) fprintf(stderr, __VA_ARGS__)
#define werr GetLastError()

static int chk_ehdr(Elf64_Ehdr *ehdr,
                    size_t st_siz, /* size of ELF file */
                    const char *what /* name of ELF file */) {
  /* NOTE: caller must ensure st_siz is at least an Ehdr */

  size_t phdr_tbl_siz;
  Elf64_Off phdr_tbl_end;

  if (ehdr->e_ident[0] != 0x7f
      || memcmp(&ehdr->e_ident[1], "ELF", 3)) {
    eprintf("%s bad magic\n", what);
fail:
    return -1;
  }

  if (ehdr->e_ident[EI_CLASS] != ELFCLASS64) {
    eprintf("fixme %s unsupported bitness\n", what);
    goto fail;
  }

#if defined(__BYTE_ORDER__) \
    && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  if (ehdr->e_ident[EI_DATA] != ELFDATA2LSB) {
#else
#error fixme
#endif
    eprintf("fixme %s unhandled endian\n", what);
    goto fail;
  }

  if (ehdr->e_ehsize < sizeof(Elf64_Ehdr)) {
    eprintf("%s hdr small\n", what);
    goto fail;
  }

  if (ehdr->e_type != ET_DYN) {
    eprintf("fixme %s not PIE\n", what);
    goto fail;
  }

#ifdef __x86_64__
  if (ehdr->e_machine != EM_X86_64) {
#else
#error fixme
#endif
    eprintf("%s wrong arch\n", what);
    goto fail;
  }

  if (ehdr->e_entry == 0) {
    eprintf("%s no entry addr\n", what);
    goto fail;
  }

  if (ehdr->e_phoff == 0 || ehdr->e_phnum == 0) {
    eprintf("%s no seg tbl\n", what);
    goto fail;
  }

  if (ehdr->e_phentsize < sizeof(Elf64_Phdr)) {
    eprintf("%s seg tbl item small\n", what);
    goto fail;
  }

  if (ehdr->e_phnum == PN_XNUM) {
    eprintf("fixme %s many segs, handling unimpled\n", what);
    goto fail;
  }

  phdr_tbl_siz = ehdr->e_phentsize * ehdr->e_phnum;

  if (iadd(ehdr->e_phoff, phdr_tbl_siz, &phdr_tbl_end)) {
    eprintf("%s seg tbl too big/far\n", what);
    goto fail;
  }

  if (st_siz < phdr_tbl_end) {
    eprintf("%s seg tbl exceeds file end\n", what);
    goto fail;
  }

  return 0;
}

/* NOTE: caller of these two func should ensure PF_R is present. */

static inline DWORD elfload_win_alloc_prot(int flags) {
  /* transform ELF segment protection value
   * into value suitable for VirtualAlloc(). */

  if (flags & PF_W) {
    if (flags & PF_X)
      return PAGE_EXECUTE_READWRITE;

    return PAGE_READWRITE;
  }

  if (flags & PF_X)
    return PAGE_EXECUTE_READ;

  return PAGE_READONLY;
}

static inline DWORD elfload_win_mmap_prot(int flags) {
  /* transform ELF segment protection value
   * into value suitable for MapViewOfFileEx(). */

  DWORD access = FILE_MAP_READ;

  if (flags & PF_W)
    access |= FILE_MAP_COPY;

  if (flags & PF_X)
    access |= FILE_MAP_EXECUTE;

  return access;
}

static inline int elfload_cpy(void *dst,
                              const void *src,
                              size_t n,
                              int prot_flags,
                              DWORD wprot) {
  bool save_restore_prot = (prot_flags & PF_W) == 0;
  DWORD dummy;

  if (save_restore_prot
      && VirtualProtect(dst, n,
                        elfload_win_alloc_prot(prot_flags | PF_W),
                        &dummy) == FALSE) {
    eprintf("err=%lu tweaking prot to cpy\n", werr);
    return -1;
  }

  memcpy(dst, src, n);

  if (save_restore_prot
      && VirtualProtect(dst, n, wprot, &dummy) == FALSE) {
    eprintf("err=%lu restoring prot after cpy\n", werr);
    return -1;
  }

  if (prot_flags & PF_X
      && FlushInstructionCache((HANDLE)-1, dst, n) == FALSE) {
    eprintf("err=%lu flushing cpu cache\n", werr);
    return -1;
  }

  return 0;
}

int load_koori(const char *path) {
  int ret = 0;
  HANDLE hFile;
  LARGE_INTEGER siz;
  HANDLE hMap = NULL;
  void *view = NULL;
  Elf64_Ehdr *ehdr;
  uintptr_t phdrs_base;
  SYSTEM_INFO si;
  DWORD the_64k;
  Elf64_Addr last_occupy;
  bool saw_1st_load_seg = false;
  Elf64_Addr continuous_load_segs_siz = 0;
  void *base;

  hFile = CreateFileA(path,
                      GENERIC_READ | GENERIC_EXECUTE,
                      FILE_SHARE_READ | FILE_SHARE_DELETE,
                      NULL,
                      OPEN_EXISTING,
                      0,
                      NULL);

  if (hFile == INVALID_HANDLE_VALUE) {
    eprintf("err=%lu opening koori.elf\n", werr);
    return -1;
  }

  if (GetFileSizeEx(hFile, &siz) == FALSE) {
    eprintf("err=%lu getting koori.elf siz\n", werr);
fail:
    ret = -1;
    goto out;
  }

  if (siz.QuadPart < sizeof(Elf64_Ehdr)) {
    eprintf("koori.elf small\n");
    goto fail;
  }

  hMap = CreateFileMappingA(hFile,
                            NULL,
                            PAGE_EXECUTE_READ,
                            0,
                            0,
                            NULL);

  if (hMap == NULL) {
    eprintf("err=%lu making map obj for koori.elf\n", werr);
    goto fail;
  }

  view = MapViewOfFile(hMap, FILE_MAP_READ,
                       0, 0, 0);

  if (view == NULL) {
    eprintf("err=%lu mapping koori.elf to peek\n", werr);
    goto fail;
  }

  ehdr = (Elf64_Ehdr *)view;

  if (chk_ehdr(ehdr, siz.QuadPart,
               "koori.elf") < 0)
    goto fail;

  phdrs_base = (uintptr_t)view + ehdr->e_phoff;

  GetSystemInfo(&si);

  the_64k = si.dwAllocationGranularity;

  for (int i = 0; i < ehdr->e_phnum; i++) {
    size_t phdr_off = i * ehdr->e_phentsize;
    Elf64_Phdr *phdr = (Elf64_Phdr *)(phdrs_base + phdr_off);
    Elf64_Off file_data_end;
    Elf64_Addr seg_start;
    Elf64_Addr seg_end;

    if (phdr->p_type == PT_INTERP) {
      eprintf("koori.elf wants dyld\n");
      goto fail;
    }

    if (phdr->p_type != PT_LOAD)
      continue;

    if (phdr->p_memsz == 0)
      continue;

    if (phdr->p_filesz > phdr->p_memsz) {
      eprintf("bad seg filesz or memsz in koori.elf\n");
      goto fail;
    }

    if (phdr->p_filesz > 0
        && (iadd(phdr->p_offset, phdr->p_filesz, &file_data_end)
            || file_data_end > siz.QuadPart)) {
      eprintf("bad seg fileoff or filesz in koori.elf\n");
      goto fail;
    }

    if (phdr->p_align % the_64k > 0) {
      eprintf("koori.elf not 64K-aligned\n");
      goto fail;
    }

    if (phdr->p_filesz > 0
        && phdr->p_offset % the_64k != phdr->p_vaddr % the_64k) {
      eprintf("koori.elf not aligned\n");
      goto fail;
    }

    if (ROUND_UP_CHKED(continuous_load_segs_siz,
                       the_64k,
                       &last_occupy)) {
      eprintf("koori.elf seg too far\n");
      goto fail;
    }

    seg_start = ROUND_DOWN(phdr->p_vaddr, the_64k);

    if (seg_start < last_occupy) {
      eprintf("koori.elf has colliding seg\n");
      goto fail;
    }

    if (!saw_1st_load_seg) {
      if (seg_start != 0x0) {
        eprintf("fixme koori.elf 1st seg not @ 0x0\n");
        goto fail;
      }

      saw_1st_load_seg = true;
    }

    if (iadd(phdr->p_vaddr, phdr->p_memsz, &seg_end)) {
      eprintf("koori.elf seg too big/far\n");
      goto fail;
    }

    if (seg_end < continuous_load_segs_siz) {
      eprintf("koori.elf seg tbl not sorted\n");
      goto fail;
    }

    continuous_load_segs_siz = seg_end;
  }

  if (continuous_load_segs_siz == 0) {
    eprintf("koori.elf nothing to load\n");
    goto fail;
  }

  if (ehdr->e_entry >= continuous_load_segs_siz) {
    eprintf("koori.elf bad entry addr\n");
    goto fail;
  }

  base = VirtualAlloc(NULL,
                      continuous_load_segs_siz,
                      MEM_RESERVE,
                      PAGE_NOACCESS);

  if (base == NULL) {
    eprintf("err=%lu finding space for koori.elf\n", werr);
    goto fail;
  }

  if (VirtualFree(base, 0, MEM_RELEASE) == FALSE) {
    eprintf("err=%lu removing dummy before mapping koori.elf\n", werr);
    goto fail;
  }

  for (int i = 0; i < ehdr->e_phnum; i++) {
    size_t phdr_off = i * ehdr->e_phentsize;
    Elf64_Phdr *phdr = (Elf64_Phdr *)(phdrs_base + phdr_off);

    if (phdr->p_type != PT_LOAD)
      continue;

    if (phdr->p_memsz == 0)
      continue;

    Elf64_Addr seg_start = ROUND_DOWN(phdr->p_vaddr, the_64k);
    uintptr_t seg_base = (uintptr_t)base + seg_start;
    size_t make_up = phdr->p_vaddr % the_64k;
    size_t memsz = phdr->p_memsz + make_up;

    if ((phdr->p_flags & PF_R) == 0) {
      if (VirtualAlloc((void *)seg_base,
                       memsz,
                       MEM_RESERVE,
                       PAGE_NOACCESS) == NULL) {
        eprintf("err=%lu reserve koori.elf seg %d\n", werr, i);
        goto fail;
      }

      continue;
    }

    if (phdr->p_filesz == 0) {
      if (VirtualAlloc((void *)seg_base,
                       memsz,
                       MEM_RESERVE | MEM_COMMIT,
                       elfload_win_alloc_prot(phdr->p_flags)) == NULL) {
        eprintf("err=%lu alloc koori.elf mono bss %d\n", werr, i);
        goto fail;
      }

      continue;
    }

    LARGE_INTEGER fileoff = {.QuadPart = ROUND_DOWN(phdr->p_offset, the_64k)};
    size_t filesz = phdr->p_filesz + make_up;

    if (filesz == memsz) {
      if (MapViewOfFileEx(hMap,
                          elfload_win_mmap_prot(phdr->p_flags),
                          fileoff.HighPart,
                          fileoff.LowPart,
                          filesz,
                          (void *)seg_base) == NULL) {
        eprintf("err=%lu mapping koori.elf seg %d (mono)\n", werr, i);
        goto fail;
      }

      continue;
    }

    DWORD alloc_wprot = elfload_win_alloc_prot(phdr->p_flags);
    size_t remainder = filesz % the_64k;

    if (remainder == 0) {
      if (MapViewOfFileEx(hMap,
                          elfload_win_mmap_prot(phdr->p_flags),
                          fileoff.HighPart,
                          fileoff.LowPart,
                          filesz,
                          (void *)seg_base) == NULL) {
        eprintf("err=%lu mapping koori.elf seg %d (simple)\n", werr, i);
        goto fail;
      }

      if (VirtualAlloc((void *)(seg_base + filesz),
                       memsz - filesz,
                       MEM_RESERVE | MEM_COMMIT,
                       alloc_wprot) == NULL) {
        eprintf("err=%lu alloc koori.elf seg %d (simple) bss\n", werr, i);
        UnmapViewOfFile((void *)seg_base);
        goto fail;
      }

      continue;
    }

    // TODO: are we allowed to copy fewer?
    uintptr_t file_bytes_base = (uintptr_t)view + fileoff.QuadPart;

    if (filesz < the_64k) {
      /* filesz too small, BSS can't go after it due to Allocation Granularity
       * instead use one longer VirtualAlloc() and copy file data to head. */

      if (VirtualAlloc((void *)seg_base,
                       memsz,
                       MEM_RESERVE | MEM_COMMIT,
                       alloc_wprot) == NULL) {
        eprintf("err=%lu alloc koori.elf seg %d (combined)\n", werr, i);
        goto fail;
      }

      if (elfload_cpy((void *)seg_base,
                      (void *)file_bytes_base,
                      filesz,
                      phdr->p_flags,
                      alloc_wprot)) {
        VirtualFree((void *)seg_base, 0, MEM_RELEASE);
        goto fail;
      }

      continue;
    }

    /* Map any requested whole 64K chunks from file directly,
     * remaining part goes to beginning of VirtualAlloc()'ed region,
     * as we can only start things at Allocation Granularity. */

    size_t rounded_filesz = filesz - remainder;

    if (MapViewOfFileEx(hMap,
                        elfload_win_mmap_prot(phdr->p_flags),
                        fileoff.HighPart,
                        fileoff.LowPart,
                        rounded_filesz,
                        (void *)seg_base) == NULL) {
      eprintf("err=%lu mapping koori.elf seg %d (-tail)\n", werr, i);
      goto fail;
    }

    uintptr_t alloc_region_base = seg_base + rounded_filesz;
    size_t bss_siz = memsz - filesz;

    if (VirtualAlloc((void *)alloc_region_base,
                     bss_siz + remainder,
                     MEM_RESERVE | MEM_COMMIT,
                     alloc_wprot) == NULL) {
      eprintf("err=%lu alloc koori.elf seg %d tail+bss\n", werr, i);
      UnmapViewOfFile((void *)seg_base);
      goto fail;
    }

    if (elfload_cpy((void *)alloc_region_base,
                    (void *)(file_bytes_base + rounded_filesz),
                    remainder,
                    phdr->p_flags,
                    alloc_wprot) < 0) {
      UnmapViewOfFile((void *)seg_base);
      VirtualFree((void *)alloc_region_base, 0, MEM_RELEASE);
      goto fail;
    }
  }

out:
  /* TODO: remove any loaded segments if failed */

  if (view != NULL)
    UnmapViewOfFile(view);

  if (hMap != NULL)
    CloseHandle(hMap);

  CloseHandle(hFile);

  return ret;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    eprintf("%s: few arg\n", argv[0]);
    return 1;
  }

  if (load_koori(argv[1]) < 0)
    return 1;

  /* TODO: unmap koori */
}
