# Special build steps for some portions of SNOW that
#  needs to be compiled with winegcc and Linux gcc.

INCS := ../../inc
OBJS := koori.o
DEPS := $(patsubst %.o,%.d,$(OBJS))
DLLS := kernel32 ucrtbase
LIBS := $(addsuffix .a,$(addprefix lib,$(DLLS)))

CPPFLAGS := $(addprefix -I,$(INCS)) -MD -MP

# TODO: can we implement stack protection?
CFLAGS := -mno-cygwin -fPIE -mno-red-zone -fno-stack-protector -Wformat -O2 -g

# TODO: implement support for -Wl,-z,pack-relative-relocs and add the flag here
LDFLAGS := $(addprefix -l,$(DLLS)) -L. -nostdlib -static-pie -Wl,-z,max-page-size=0x10000 -e koori_entry -T snowin.ld

koori.elf: $(OBJS) $(LIBS)
	gcc $(OBJS) $(LDFLAGS) -o $@

-include $(DEPS)

%.o: %.c
	winegcc $(CPPFLAGS) $(CFLAGS) -c $< -o $@

lib%.a: %.lst
	./snowin_gen_stub.py $<

clean:
	rm -f koori.elf $(OBJS) $(DEPS) $(LIBS)

.PHONY: clean
