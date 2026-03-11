ARCHITE = x86_64

CC      = gcc
LD      = ld
AS      = nasm
OBJCOPY = objcopy

CFLAGS = -m64 -ffreestanding -fno-pic -fno-pie -fno-stack-protector -mno-red-zone -nostdlib -nostartfiles -c
KPROCESS_POLL_BUDGET ?= 4
CFLAGS += -DKPROCESS_POLL_BUDGET=$(KPROCESS_POLL_BUDGET)
CFLAGS += -I$(INCLUDE) -I$(INCLUDE)/APISLua

LDFLAGS = -m elf_$(ARCHITE) -T linker.ld

BUILD = build
INCLUDE = include
DRIVES = drives

BOOTLOADER = core/bootloader.asm
KENTRY	   = core/kernel_entry.asm
KERNEL	   = kernel.c

CFILES_INCLUDE := $(wildcard include/*.c) $(wildcard include/APISLua/*.c)
CFILES_DRIVES  := $(wildcard drives/*.c)
CFILES   := $(CFILES_INCLUDE) $(CFILES_DRIVES)
ASMFILES := $(wildcard include/*.asm)

LUA_SRC = lua/src
LUA_FILES = lapi.c lcode.c lctype.c ldebug.c ldo.c ldump.c lfunc.c lgc.c llex.c lmem.c lobject.c lopcodes.c lparser.c lstate.c lstring.c ltable.c ltm.c lundump.c lvm.c lzio.c
LUA_OBJS := $(patsubst %.c,$(BUILD)/lua_%.o,$(LUA_FILES))
KCMDS_LUA = include/APISLua/commands.lua
KCMDS_INC = include/APISLua/commands_lua.inc

# idt_config.asm antigo ainda esta em 32-bit e nao e mais necessario.
ASMFILES := $(filter-out include/idt_config.asm,$(ASMFILES))

OBJS_C_INCLUDE := $(patsubst include/%.c,$(BUILD)/%.o,$(CFILES_INCLUDE))
OBJS_C_DRIVES  := $(patsubst drives/%.c,$(BUILD)/%.o,$(CFILES_DRIVES))
OBJS_C         := $(OBJS_C_INCLUDE) $(OBJS_C_DRIVES)
OBJS_ASM := $(patsubst include/%.asm,$(BUILD)/%.o,$(ASMFILES))

all: compile

compile: $(BUILD)/kernel.img

$(BUILD):
	mkdir -p $(BUILD)

# Core
$(BUILD)/kernel_entry.o: $(KENTRY) | $(BUILD)
	$(AS) -f elf64 $< -o $@

# Kernel
$(BUILD)/kernel.o: $(KERNEL) | $(BUILD)
	$(CC) $(CFLAGS) $< -o $@

$(KCMDS_INC): $(KCMDS_LUA)
	@awk '{gsub(/\\/,"\\\\"); gsub(/"/,"\\\""); print "\"" $$0 "\\n\""}' $< > $@

$(BUILD)/APISLua/klua_cmds.o: include/APISLua/klua_cmds.c $(KCMDS_INC) | $(BUILD)
	$(CC) $(CFLAGS) $< -o $@


# Other dependencies
$(BUILD)/%.o: include/%.c | $(BUILD)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $< -o $@

$(BUILD)/%.o: drives/%.c | $(BUILD)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(INCLUDE) $< -o $@

$(BUILD)/lua_%.o: $(LUA_SRC)/%.c | $(BUILD)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(INCLUDE) -I$(LUA_SRC) $< -o $@

$(BUILD)/%.o: include/%.asm | $(BUILD)
	mkdir -p $(dir $@)
	$(AS) -f elf64 $< -o $@

# Mounting IMG
$(BUILD)/kernel.elf: $(BUILD)/kernel_entry.o $(BUILD)/kernel.o $(OBJS_C) $(OBJS_ASM) $(LUA_OBJS)
	$(LD) $(LDFLAGS) $^ -o $@

$(BUILD)/kernel.bin: $(BUILD)/kernel.elf
	$(OBJCOPY) -O binary $< $@

$(BUILD)/bootloader.bin: $(BOOTLOADER) $(BUILD)/kernel.bin | $(BUILD)
	BYTES=$$(stat -c%s $(BUILD)/kernel.bin); \
	SECTORS=$$((($$BYTES + 511) / 512)); \
	$(AS) -f bin -DKERNEL_SECTORS=$$SECTORS -DKERNEL_BYTES=$$BYTES $< -o $@

$(BUILD)/kernel.img: $(BUILD)/bootloader.bin $(BUILD)/kernel.bin
	cat $^ > $@
	SECTORS=$$((($$(stat -c%s $(BUILD)/kernel.bin) + 511) / 512)); \
	truncate -s $$(((1 + $$SECTORS) * 512)) $@


run: $(BUILD)/kernel.img
	qemu-system-$(ARCHITE) -drive format=raw,file=$< -serial stdio -smp 2

clean:
	rm -rf $(BUILD)