ifeq ($(strip $(RGBDS)),)
    $(error "Please set RGBDS in your environment. export RGBDS=<path to rgbds>")
endif

BINS = bios.bin dummy_bios.bin
SIZE = 2304

AS = $(RGBDS)/rgbasm
LINK = $(RGBDS)/rgblink
TRUNCATE = truncate

all: $(BINS)

clean:
	rm -f *.o *.bin

%.o: %.s
	$(AS) -o $@ $<

%.bin: %.o
	$(LINK) -o $@ $<
	$(TRUNCATE) -s $(SIZE) $@
