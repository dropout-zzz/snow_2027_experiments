SECTIONS {
  .snowinlibs : {
    __snowinlibs = .;
    KEEP(*(.snowinlibs))
    __snowinlibs_end = .;
  }
  .snowinfuncs : {
    __snowinfuncs = .;
    KEEP(*(.snowinfuncs))
    __snowinfuncs_end = .;
  }
}
INSERT AFTER .data.rel.ro;
