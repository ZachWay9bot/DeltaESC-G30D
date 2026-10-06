CC := clang
OBJCOPY := llvm-objcopy
OBJDUMP := llvm-objdump
CFLAGS_COMMON = --target=arm-none-eabi -mcpu=cortex-m3 -mthumb -ffreestanding -fno-builtin -fdata-sections -ffunction-sections -Oz -Wall -Wextra -Werror -Isrc
LDFLAGS_COMMON = --target=arm-none-eabi -mcpu=cortex-m3 -mthumb -nostdlib -Wl,-T,linker.ld,--gc-sections
SRC = src/main.c src/sensorless_control.c

SAFE_ELF = build/DeltaESC_G30D_v0_5_sensorless_syncsafe.elf
SAFE_BIN = build/DeltaESC_G30D_v0_5_sensorless_syncsafe.bin
ACTIVE_ELF = build/DeltaESC_G30D_v0_5_observer_zero_vector.elf
ACTIVE_BIN = build/DeltaESC_G30D_v0_5_observer_zero_vector.bin
BENCH_ELF = build/DeltaESC_G30D_v0_5_sensorless_bench.elf
BENCH_BIN = build/DeltaESC_G30D_v0_5_sensorless_bench.bin

all: safe active bench
safe: $(SAFE_BIN)
active: $(ACTIVE_BIN)
bench: $(BENCH_BIN)

build:
	mkdir -p build

$(SAFE_ELF): $(SRC) linker.ld | build
	$(CC) $(CFLAGS_COMMON) -DPOWER_STAGE_ARM_ALLOWED=0 -DSENSORLESS_RUN_ALLOWED=0 $(SRC) $(LDFLAGS_COMMON) -Wl,-Map=build/v0_5_syncsafe.map -o $@

$(SAFE_BIN): $(SAFE_ELF)
	$(OBJCOPY) -O binary $< $@
	$(OBJDUMP) -h $< > build/v0_5_syncsafe_sections.txt
	$(OBJDUMP) -d $< > build/v0_5_syncsafe_disassembly.txt

$(ACTIVE_ELF): $(SRC) linker.ld | build
	$(CC) $(CFLAGS_COMMON) -DPOWER_STAGE_ARM_ALLOWED=1 -DSENSORLESS_RUN_ALLOWED=0 $(SRC) $(LDFLAGS_COMMON) -Wl,-Map=build/v0_5_observer.map -o $@

$(ACTIVE_BIN): $(ACTIVE_ELF)
	$(OBJCOPY) -O binary $< $@
	$(OBJDUMP) -h $< > build/v0_5_observer_sections.txt
	$(OBJDUMP) -d $< > build/v0_5_observer_disassembly.txt

$(BENCH_ELF): $(SRC) linker.ld | build
	$(CC) $(CFLAGS_COMMON) -DPOWER_STAGE_ARM_ALLOWED=1 -DSENSORLESS_RUN_ALLOWED=1 $(SRC) $(LDFLAGS_COMMON) -Wl,-Map=build/v0_5_bench.map -o $@

$(BENCH_BIN): $(BENCH_ELF)
	$(OBJCOPY) -O binary $< $@
	$(OBJDUMP) -h $< > build/v0_5_bench_sections.txt
	$(OBJDUMP) -d $< > build/v0_5_bench_disassembly.txt

clean:
	rm -rf build
