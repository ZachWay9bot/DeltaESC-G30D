CC := clang
OBJCOPY := llvm-objcopy
OBJDUMP := llvm-objdump
CFLAGS_COMMON = --target=arm-none-eabi -mcpu=cortex-m3 -mthumb -ffreestanding -fno-builtin -fdata-sections -ffunction-sections -Oz -Wall -Wextra -Werror -Isrc
LDFLAGS_COMMON = --target=arm-none-eabi -mcpu=cortex-m3 -mthumb -nostdlib -Wl,-T,linker.ld,--gc-sections
SRC = src/main.c src/control_diag.c src/ninebot_diag.c

SAFE_ELF = build/DeltaESC_G30D_v0_4_1_ble_syncsafe.elf
SAFE_BIN = build/DeltaESC_G30D_v0_4_1_ble_syncsafe.bin
ACTIVE_ELF = build/DeltaESC_G30D_v0_4_1_ble_activecapable.elf
ACTIVE_BIN = build/DeltaESC_G30D_v0_4_1_ble_activecapable.bin

all: safe active
safe: $(SAFE_BIN)
active: $(ACTIVE_BIN)

build:
	mkdir -p build

$(SAFE_ELF): $(SRC) linker.ld | build
	$(CC) $(CFLAGS_COMMON) -DPOWER_STAGE_ARM_ALLOWED=0 $(SRC) $(LDFLAGS_COMMON) -Wl,-Map=build/v0_4_syncsafe.map -o $@

$(SAFE_BIN): $(SAFE_ELF)
	$(OBJCOPY) -O binary $< $@
	$(OBJDUMP) -h $< > build/v0_4_syncsafe_sections.txt
	$(OBJDUMP) -d $< > build/v0_4_syncsafe_disassembly.txt

$(ACTIVE_ELF): $(SRC) linker.ld | build
	$(CC) $(CFLAGS_COMMON) -DPOWER_STAGE_ARM_ALLOWED=1 $(SRC) $(LDFLAGS_COMMON) -Wl,-Map=build/v0_4_active.map -o $@

$(ACTIVE_BIN): $(ACTIVE_ELF)
	$(OBJCOPY) -O binary $< $@
	$(OBJDUMP) -h $< > build/v0_4_active_sections.txt
	$(OBJDUMP) -d $< > build/v0_4_active_disassembly.txt

clean:
	rm -rf build
