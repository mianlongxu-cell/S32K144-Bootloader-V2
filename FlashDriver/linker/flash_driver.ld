ENTRY(FlashDriver_Init)
MEMORY { driver (RWX) : ORIGIN = 0x20005800, LENGTH = 0x800 }
SECTIONS {
  .api : { KEEP(*(.driver_api)) } > driver
  .text : { *(.text*) *(.rodata*) . = ALIGN(4); } > driver
  .data : { *(.data*) . = ALIGN(4); } > driver
  __driver_image_end = .;
  .bss (NOLOAD) : { *(.bss*) *(COMMON) . = ALIGN(4); } > driver
  __driver_end = .;
  .ARM.attributes 0 : { *(.ARM.attributes) }
  /DISCARD/ : { *(.comment*) *(.eh_frame*) *(.ARM.exidx*) *(.ARM.extab*) }
  ASSERT(ADDR(.api) == 0x20005800, "Driver API moved")
  ASSERT(__driver_end <= 0x20006000, "Driver exceeds SRAM reservation")
}
