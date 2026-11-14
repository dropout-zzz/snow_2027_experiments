# Special build steps for some portions of SNOW that
#  needs to be compiled with winegcc and Linux gcc.

INCS := ../../inc
OBJS := koori.o
DEPS := $(patsubst %.o,%.d,$(OBJS))

CPPFLAGS := $(addprefix -I,$(INCS)) -MD -MP
CFLAGS := -mno-cygwin -fPIE -mno-red-zone
LDFLAGS := -nostdlib -static-pie -Wl,-z,max-page-size=0x10000 -e koori_entry

koori.elf: $(OBJS)
	gcc $(LDFLAGS) $^ -o $@

-include $(DEPS)

%.o: %.c
	winegcc $(CPPFLAGS) $(CFLAGS) -c $< -o $@

clean:
	rm -f koori.elf $(OBJS) $(DEPS)

.PHONY: clean
