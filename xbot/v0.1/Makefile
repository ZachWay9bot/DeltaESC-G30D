CC ?= clang
OBJCOPY ?= llvm-objcopy
OBJDUMP ?= llvm-objdump
CFLAGS = --target=arm-none-eabi -mcpu=cortex-m3 -mthumb -ffreestanding -fno-builtin -fdata-sections -ffunction-sections -Oz -Wall -Wextra -Werror -Isrc
LDFLAGS = --target=arm-none-eabi -mcpu=cortex-m3 -mthumb -nostdlib -Wl,-T,linker.ld,--gc-sections
SRC=src/main.c src/bemf_detector.c
ELF=build/XBOT_SENSORLESS_PASSIVE_BEMF_PROBE_v0.1.elf
BIN=build/XBOT_SENSORLESS_PASSIVE_BEMF_PROBE_v0.1.bin
all: $(BIN)
build: ; mkdir -p build
$(ELF): $(SRC) linker.ld | build
	$(CC) $(CFLAGS) $(SRC) $(LDFLAGS) -Wl,-Map=build/XBOT_SENSORLESS_PASSIVE_BEMF_PROBE_v0.1.map -o $@
$(BIN): $(ELF)
	$(OBJCOPY) -O binary $< $@
	$(OBJDUMP) -h $< > build/sections.txt
	$(OBJDUMP) -d $< > build/disassembly.txt
clean: ; rm -rf build
