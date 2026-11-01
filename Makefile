# Makefile for ps4_volume_control GoldHEN Plugin

TARGET       := ps4_volume_control
TARGET_ELF   := $(TARGET).elf
TARGET_PRX   := $(TARGET).prx

# OpenOrbis & GoldHEN SDK paths
TOOLCHAIN    ?= $(OO_PS4_TOOLCHAIN)
GH_SDK       ?= $(GOLDHEN_SDK)

PROJDIR      := source
INCLUDEDIR   := include
INTDIR       := build

# Libraries linked into the PRX
LIBS         := -lSceLibcInternal -lGoldHEN_Hook -lkernel -lSceSysmodule -lScePad -lSceAudioOut

# Compiler & flags
CC           := clang
LD           := ld.lld

CFLAGS       := --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -Wall -O2 \
                -isysroot $(TOOLCHAIN) -isystem $(TOOLCHAIN)/include \
                -I$(GH_SDK)/include -I$(INCLUDEDIR)

LDFLAGS      := -m elf_x86_64 -pie --script $(TOOLCHAIN)/link.x -e _init --eh-frame-hdr \
                -L$(TOOLCHAIN)/lib -L$(GH_SDK) $(LIBS)

CFILES       := $(wildcard $(PROJDIR)/*.c)
OBJS         := $(patsubst $(PROJDIR)/%.c, $(INTDIR)/%.o, $(CFILES))

$(INTDIR):
	mkdir -p $(INTDIR)

$(INTDIR)/%.o: $(PROJDIR)/%.c | $(INTDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(TARGET_PRX): $(INTDIR) $(OBJS)
	$(LD) $(GH_SDK)/build/crtprx.o $(OBJS) -o $(TARGET_ELF) $(LDFLAGS)
	$(TOOLCHAIN)/bin/linux/create-fself -in=$(TARGET_ELF) -out=$(TARGET).oelf --lib=$(TARGET_PRX) --paid 0x3800000000000011

all: $(TARGET_PRX)

clean:
	rm -rf $(INTDIR) $(TARGET_ELF) $(TARGET).oelf $(TARGET_PRX)

.PHONY: all clean

