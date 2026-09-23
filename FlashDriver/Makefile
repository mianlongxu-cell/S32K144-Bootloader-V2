CC := arm-none-eabi-gcc
OBJCOPY := arm-none-eabi-objcopy
SIZE := arm-none-eabi-size
FLAGS := -mcpu=cortex-m4 -mthumb -mfloat-abi=softfp -mfpu=fpv4-sp-d16 -std=c11 -Os -g3 -Wall -Wextra -Werror -ffreestanding -fno-builtin -fno-stack-protector -ffunction-sections -fdata-sections -DCPU_S32K144HFT0VLLT -I../include -I../Bootloader/inc
.PHONY: all clean
all: build/flash_driver.elf build/flash_driver.bin
build/flash_driver.o: src/flash_driver.c ../include/flash_driver_api.h ../Bootloader/inc/boot_config.h
	@mkdir -p build
	$(CC) $(FLAGS) -MMD -MP -c $< -o $@
build/flash_driver.elf: build/flash_driver.o linker/flash_driver.ld
	$(CC) $(FLAGS) -nostdlib -Wl,--gc-sections -Wl,-Map,build/flash_driver.map -T linker/flash_driver.ld $< -o $@
	$(SIZE) $@
build/flash_driver.bin: build/flash_driver.elf
	$(OBJCOPY) -O binary $< $@
clean:
	rm -rf build
-include build/flash_driver.d

