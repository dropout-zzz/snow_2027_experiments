typedef struct {
  void             *Handle;
  const char       *Name;
} SNOWIN_LIBRARY,  *PSNOWIN_LIBRARY;

typedef struct {
  void             *Address;
  const char       *Name;
  PSNOWIN_LIBRARY  Library;
} SNOWIN_FUNCTION, *PSNOWIN_FUNCTION;

#ifdef BUILDING_STUB_LIB

#define ATTR       __attribute__

#define HIDDEN     visibility("hidden")

#define LIB_SECT   section(".snowinlibs")
#define FUNC_SECT  section(".snowinfuncs")

#define ALIGN(T)   aligned(__alignof__(T))

#define DEFINE_LBRY(x)                          \
  ATTR((LIB_SECT,HIDDEN,ALIGN(SNOWIN_LIBRARY))) \
  SNOWIN_LIBRARY __snowinlib_ ## x = {          \
    .Name    = #x ".dll",                       \
  };

#ifdef __x86_64__
#define TRAMPO_IMPL(x)                          \
  ATTR((naked,HIDDEN))                          \
  void x(void) {                                \
    __asm__("jmp *__snowinfunc_" #x "(%rip)");  \
  }
#endif

#define DEFINE_FUNC(a,b)                        \
  extern SNOWIN_LIBRARY __snowinlib_ ## a;      \
                                                \
  ATTR((FUNC_SECT,used,ALIGN(SNOWIN_FUNCTION))) \
  static SNOWIN_FUNCTION __snowinfunc_ ## b = { \
    .Name    = #b,                              \
    .Library = &__snowinlib_ ## a,              \
  };                                            \
                                                \
  TRAMPO_IMPL(b)

#endif /* BUILDING_STUB_LIB */
