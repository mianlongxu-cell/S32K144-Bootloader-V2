CC := arm-none-eabi-gcc
FAULT_INJECTION ?= 0
OBJCOPY := arm-none-eabi-objcopy
SIZE := arm-none-eabi-size

TARGET := build/Bootloader
CPU_FLAGS := -mcpu=cortex-m4 -mthumb -mfloat-abi=softfp -mfpu=fpv4-sp-d16
CFLAGS := $(CPU_FLAGS) -std=c11 -O0 -g3 -Wall -Wextra -Werror \
          -ffreestanding -ffunction-sections -fdata-sections \
          -DCPU_S32K144HFT0VLLT -DBOOT_ENABLE_FAULT_INJECTION=$(FAULT_INJECTION) -Iinc -I../include
ASFLAGS := $(CPU_FLAGS) -g3 -x assembler-with-cpp
LDFLAGS := $(CPU_FLAGS) -nostdlib -Wl,--gc-sections \
           -Wl,-Map,$(TARGET).map -T linker/S32K144_bootloader.ld

C_SOURCES := $(wildcard src/*.c)
C_OBJECTS := $(patsubst src/%.c,build/%.o,$(C_SOURCES))
COMMON_OBJECTS := build/Uja1169.o
ASM_OBJECTS := build/startup_boot.o build/flash_driver_blob.o
OBJECTS := $(C_OBJECTS) $(COMMON_OBJECTS) $(ASM_OBJECTS)

.PHONY: all clean pbl-flexnvm

all: $(TARGET).elf $(TARGET).srec

pbl-flexnvm: build/PBL_FlexNVM.elf build/PBL_FlexNVM.srec

$(TARGET).elf: $(OBJECTS) linker/S32K144_bootloader.ld
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS) -lgcc
	$(SIZE) --format=berkeley $@

$(TARGET).srec: $(TARGET).elf
	$(OBJCOPY) -O srec $< $@

build/PBL_FlexNVM.elf: $(OBJECTS) linker/S32K144_pbl_flexnvm.ld
	$(CC) $(CPU_FLAGS) -nostdlib -Wl,--gc-sections \
		-Wl,-Map,build/PBL_FlexNVM.map \
		-T linker/S32K144_pbl_flexnvm.ld -o $@ $(OBJECTS) -lgcc
	$(SIZE) --format=berkeley $@

build/PBL_FlexNVM.srec: build/PBL_FlexNVM.elf
	$(OBJCOPY) -O srec $< $@

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

build/Uja1169.o: ../src/Uja1169.c | build
	$(CC) $(CFLAGS) -MMD -c $< -o $@

build/startup_boot.o: startup/startup_boot.S | build
	$(CC) $(ASFLAGS) -c $< -o $@

.PHONY: driver
driver:
	$(MAKE) -C ../FlashDriver all

build/flash_driver_blob.o: startup/flash_driver_blob.S driver | build
	$(CC) $(ASFLAGS) -c $< -o $@

build:
	mkdir -p build

clean:
	rm -rf build

-include $(C_OBJECTS:.o=.d)
