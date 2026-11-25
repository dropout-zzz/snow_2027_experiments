#include "koori.h"
#include "_snowin.h"

extern SNOWIN_LIBRARY __snowinlibs[];
extern char __snowinlibs_end;

extern SNOWIN_FUNCTION __snowinfuncs[];
extern char __snowinfuncs_end;

__stdcall void koori_entry(struct koori_entry_params *args) {
  for (PSNOWIN_LIBRARY library = __snowinlibs; (char *)library != &__snowinlibs_end; library++) {
    library->Handle = args->load_lib_cb(library->Name);
  }

  for (PSNOWIN_FUNCTION function = __snowinfuncs; (char *)function != &__snowinfuncs_end; function++) {
    function->Address = args->get_fn_addr_cb(function->Library->Handle, function->Name);
  }
}
