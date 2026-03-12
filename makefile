# ---- Architecture selection ----
# Build for x86_64 (default) or aarch64:
#   make              → x86_64
#   make ARCH=aarch64 → aarch64
ARCH ?= x86_64

BUILD = build
INCLUDE = include
DRIVES = drives
STORAGE = $(BUILD)/storage.img
STORAGE_SIZE = 4M

# ---- Toolchain per architecture ----
ifeq ($(ARCH),aarch64)
  ARCHITE = aarch64
  CC      = aarch64-linux-gnu-gcc
  LD      = aarch64-linux-gnu-ld
  OBJCOPY = aarch64-linux-gnu-objcopy
  AS      = aarch64-linux-gnu-as
  CFLAGS  = -ffreestanding -fno-pic -fno-pie -fno-stack-protector \
            -nostdlib -nostartfiles -c -march=armv8-a
  LDFLAGS = -T core/linker_aarch64.ld
  BOOTLOADER =
  KENTRY = core/boot_aarch64.S
else
  ARCHITE = x86_64
  CC      = gcc
  LD      = ld
  OBJCOPY = objcopy
  AS_NASM = nasm
  CFLAGS  = -m64 -ffreestanding -fno-pic -fno-pie -fno-stack-protector \
            -mno-red-zone -nostdlib -nostartfiles -c
  LDFLAGS = -m elf_$(ARCHITE) -T linker.ld
  BOOTLOADER = core/bootloader.asm
  KENTRY = core/kernel_entry.asm
endif

KPROCESS_POLL_BUDGET ?= 4
CFLAGS += -DKPROCESS_POLL_BUDGET=$(KPROCESS_POLL_BUDGET)
CFLAGS += -I$(INCLUDE) -I$(INCLUDE)/APISLua

KERNEL = kernel.c

# ---- Source file selection per architecture ----
# Collect all C files then filter based on target arch.
ALL_C_INCLUDE := $(wildcard include/*.c) $(wildcard include/APISLua/*.c)
ALL_C_DRIVES  := $(wildcard drives/*.c)
CFILES_FS     := $(wildcard fs/*/*.c)

ifeq ($(ARCH),aarch64)
  # Keep *_aarch64.c, exclude x86-only: idt.c, ktimer.c, ksys.c, setjmp.asm, isr.asm
  CFILES_INCLUDE := $(filter-out include/idt.c include/ktimer.c include/ksys.c,$(ALL_C_INCLUDE))
  CFILES_DRIVES  := $(filter     %_aarch64.c,$(ALL_C_DRIVES))
else
  # Keep x86 files, exclude *_aarch64.c
  CFILES_INCLUDE := $(filter-out %_aarch64.c,$(ALL_C_INCLUDE))
  CFILES_DRIVES  := $(filter-out %_aarch64.c,$(ALL_C_DRIVES))
endif

CFILES := $(CFILES_INCLUDE) $(CFILES_DRIVES) $(CFILES_FS)

# ---- Assembly files ----
ifeq ($(ARCH),aarch64)
  # Only the setjmp aarch64 assembly
  ASMFILES_S := include/setjmp_aarch64.S
  ASMFILES_NASM :=
else
  ASMFILES_NASM := $(wildcard include/*.asm)
  ASMFILES_NASM := $(filter-out include/idt_config.asm,$(ASMFILES_NASM))
  ASMFILES_S :=
endif

# ---- Lua sources ----
LUA_SRC = lua/src
LUA_FILES = lapi.c lcode.c lctype.c ldebug.c ldo.c ldump.c lfunc.c lgc.c \
            llex.c lmem.c lobject.c lopcodes.c lparser.c lstate.c lstring.c \
            ltable.c ltm.c lundump.c lvm.c lzio.c
LUA_OBJS := $(patsubst %.c,$(BUILD)/lua_%.o,$(LUA_FILES))
KCMDS_LUA = include/APISLua/commands.lua
KCMDS_INC = include/APISLua/commands_lua.inc

# ---- Object file lists ----
OBJS_C_INCLUDE := $(patsubst include/%.c,$(BUILD)/%.o,$(CFILES_INCLUDE))
OBJS_C_DRIVES  := $(patsubst drives/%.c,$(BUILD)/%.o,$(CFILES_DRIVES))
OBJS_C_FS      := $(patsubst fs/%.c,$(BUILD)/fs_%.o,$(CFILES_FS))
OBJS_C         := $(OBJS_C_INCLUDE) $(OBJS_C_DRIVES) $(OBJS_C_FS)
OBJS_ASM_NASM  := $(patsubst include/%.asm,$(BUILD)/%.o,$(ASMFILES_NASM))
OBJS_ASM_S     := $(patsubst include/%.S,$(BUILD)/%.o,$(ASMFILES_S))
OBJS_ASM       := $(OBJS_ASM_NASM) $(OBJS_ASM_S)

# ============================================================
all: compile

compile: $(BUILD)/kernel.img

$(BUILD):
	mkdir -p $(BUILD)

# ---- Core entry point ----
ifeq ($(ARCH),aarch64)
$(BUILD)/kernel_entry.o: $(KENTRY) | $(BUILD)
	$(CC) -ffreestanding -nostdlib -c $< -o $@
else
$(BUILD)/kernel_entry.o: $(KENTRY) | $(BUILD)
	$(AS_NASM) -f elf64 $< -o $@
endif

# ---- Kernel main ----
$(BUILD)/kernel.o: $(KERNEL) | $(BUILD)
	$(CC) $(CFLAGS) $< -o $@

# ---- Lua commands embed ----
$(KCMDS_INC): $(KCMDS_LUA)
	@awk '{gsub(/\\/,"\\\\"); gsub(/"/,"\\\""); print "\"" $$0 "\\n\""}' $< > $@

$(BUILD)/APISLua/klua_cmds.o: include/APISLua/klua_cmds.c $(KCMDS_INC) | $(BUILD)
	$(CC) $(CFLAGS) $< -o $@

# ---- C compilation rules ----
$(BUILD)/%.o: include/%.c | $(BUILD)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $< -o $@

$(BUILD)/%.o: drives/%.c | $(BUILD)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(INCLUDE) $< -o $@

$(BUILD)/fs_%.o: fs/%.c | $(BUILD)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(INCLUDE) -Ifs $< -o $@

$(BUILD)/lua_%.o: $(LUA_SRC)/%.c | $(BUILD)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(INCLUDE) -I$(LUA_SRC) $< -o $@

# ---- Assembly compilation rules ----
ifneq ($(ASMFILES_NASM),)
$(BUILD)/%.o: include/%.asm | $(BUILD)
	mkdir -p $(dir $@)
	$(AS_NASM) -f elf64 $< -o $@
endif

ifneq ($(ASMFILES_S),)
$(BUILD)/%.o: include/%.S | $(BUILD)
	mkdir -p $(dir $@)
	$(CC) -ffreestanding -nostdlib -c $< -o $@
endif

# ---- Linking ----
$(BUILD)/kernel.elf: $(BUILD)/kernel_entry.o $(BUILD)/kernel.o $(OBJS_C) $(OBJS_ASM) $(LUA_OBJS)
	$(LD) $(LDFLAGS) $^ -o $@

$(BUILD)/kernel.bin: $(BUILD)/kernel.elf
	$(OBJCOPY) -O binary $< $@

# ---- Disk image (x86_64 only needs bootloader; aarch64 uses raw kernel.bin) ----
ifeq ($(ARCH),aarch64)
$(BUILD)/kernel.img: $(BUILD)/kernel.bin
	cp $< $@
else
$(BUILD)/bootloader.bin: $(BOOTLOADER) $(BUILD)/kernel.bin | $(BUILD)
	BYTES=$$(stat -c%s $(BUILD)/kernel.bin); \
	SECTORS=$$((($$BYTES + 511) / 512)); \
	$(AS_NASM) -f bin -DKERNEL_SECTORS=$$SECTORS -DKERNEL_BYTES=$$BYTES $< -o $@

$(BUILD)/kernel.img: $(BUILD)/bootloader.bin $(BUILD)/kernel.bin
	cat $^ > $@
	SECTORS=$$((($$(stat -c%s $(BUILD)/kernel.bin) + 511) / 512)); \
	truncate -s $$(((1 + $$SECTORS) * 512)) $@
endif

# ---- Run targets ----
ifeq ($(ARCH),aarch64)
run: $(BUILD)/kernel.img
	qemu-system-aarch64 -machine virt -cpu cortex-a53 -m 128M \
		-kernel $< -nographic
else
run: $(BUILD)/kernel.img $(STORAGE)
	qemu-system-$(ARCHITE) -drive format=raw,file=$< \
		-drive format=raw,file=$(STORAGE),if=ide,index=1 \
		-serial stdio -smp 2 -m 2G
endif

$(STORAGE):
	dd if=/dev/zero of=$@ bs=1 count=0 seek=$(STORAGE_SIZE) 2>/dev/null

# --- Unit tests (host-compiled) ---
TEST_CC = gcc
TEST_CFLAGS = -Wall -Wextra -g -O0 -m64 -Iinclude
TEST_DIR = tests
TEST_BUILD = $(BUILD)/tests
TEST_SRCS = $(wildcard $(TEST_DIR)/test_*.c)
TEST_BINS = $(patsubst $(TEST_DIR)/test_%.c,$(TEST_BUILD)/test_%,$(TEST_SRCS))

$(TEST_BUILD):
	mkdir -p $@

$(TEST_BUILD)/test_%: $(TEST_DIR)/test_%.c $(wildcard $(TEST_DIR)/*.h) | $(TEST_BUILD)
	$(TEST_CC) $(TEST_CFLAGS) $< -o $@ -lm

test: $(TEST_BINS)
	@passed=0; failed=0; total=0; \
	for t in $(TEST_BINS); do \
		total=$$((total + 1)); \
		name=$$(basename $$t); \
		echo ""; echo ">>> Running $$name..."; \
		if $$t; then \
			passed=$$((passed + 1)); \
		else \
			failed=$$((failed + 1)); \
			echo "!!! $$name FAILED"; \
		fi; \
	done; \
	echo ""; echo "=== Test Summary: $$passed/$$total passed, $$failed failed ==="; \
	[ $$failed -eq 0 ]

clean:
	rm -rf $(BUILD)