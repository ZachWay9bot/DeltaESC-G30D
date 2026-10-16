CC ?= clang
OBJCOPY ?= llvm-objcopy
OBJDUMP ?= llvm-objdump
CFLAGS = --target=arm-none-eabi -mcpu=cortex-m3 -mthumb -ffreestanding -fno-builtin -fdata-sections -ffunction-sections -Oz -Wall -Wextra -Werror -Isrc
LDFLAGS = --target=arm-none-eabi -mcpu=cortex-m3 -mthumb -nostdlib -Wl,-T,linker.ld,--gc-sections
SRC = src/main.c src/bemf_detector.c src/flash.c src/ninebot_tea.c src/uart.c src/protocol.c src/iap.c
ELF = build/XBOT_SENSORLESS_PASSIVE_IAP_v0.2.elf
BIN = build/XBOT_SENSORLESS_PASSIVE_IAP_v0.2.bin
ENC = build/ESC2_XBOT_SL02_v0.2_Driver_FIRM.bin.enc
all: $(ENC)
build: ; mkdir -p build
$(ELF): $(SRC) linker.ld | build
	$(CC) $(CFLAGS) $(SRC) $(LDFLAGS) -Wl,-Map=build/XBOT_SENSORLESS_PASSIVE_IAP_v0.2.map -o $@
$(BIN): $(ELF)
	$(OBJCOPY) -O binary $< $@
	$(OBJDUMP) -h $< > build/sections.txt
	$(OBJDUMP) -d $< > build/disassembly.txt
$(ENC): $(BIN) tools/ninebot_pack.py
	python3 tools/ninebot_pack.py pack $(BIN) $(ENC)
clean: ; rm -rf build
