ENTRY(Reset_Handler)

STACK_SIZE = 0x00000FF0;
/* Absolute diagnostic symbols only: NOT loadable/erasable ELF segments. */
__JournalCopy0 = 0x00078000;
__JournalCopy1 = 0x00079000;
__JournalSectorSize = 0x1000;
__MetadataA0 = 0x0007A000;
__MetadataA1 = 0x0007B000;
__MetadataB0 = 0x0007C000;
__MetadataB1 = 0x0007D000;
ASSERT(__JournalCopy1 + __JournalSectorSize <= 0x0007F000, "Journal overlaps legacy metadata")
ASSERT(__MetadataB1 + __JournalSectorSize <= 0x0007E000, "Metadata overlaps reserved sector")

MEMORY
{
  m_interrupts   (RX) : ORIGIN = 0x00000000, LENGTH = 0x00000400
  m_flash_config (RX) : ORIGIN = 0x00000400, LENGTH = 0x00000010
  m_text         (RX) : ORIGIN = 0x00000410, LENGTH = 0x00007BF0
  /* Final 16 bytes are retained across software reset for Boot Request. */
  m_sram         (RW) : ORIGIN = 0x1FFF8000, LENGTH = 0x0000D800
  m_driver       (RWX): ORIGIN = 0x20005800, LENGTH = 0x00000800
  m_stack        (RW) : ORIGIN = 0x20006000, LENGTH = 0x00000FF0
}

SECTIONS
{
  .interrupts :
  {
    __VECTOR_TABLE = .;
    KEEP(*(.isr_vector))
  } > m_interrupts

  .flash_config :
  {
    KEEP(*(.FlashConfig))
  } > m_flash_config

  .text :
  {
    . = ALIGN(4);
    *(.text.Reset_Handler)
    *(.text*)
    *(.rodata*)
    *(.glue_7)
    *(.glue_7t)
    . = ALIGN(4);
    __boot_flash_end__ = .;
  } > m_text

  .ARM.extab : { *(.ARM.extab*) } > m_text
  .ARM.exidx : { *(.ARM.exidx*) } > m_text

  __DATA_ROM = LOADADDR(.data);
  .data :
  {
    . = ALIGN(4);
    __DATA_RAM = .;
    *(.data*)
    *(.ramfunc*)
    . = ALIGN(4);
    __DATA_END = .;
  } > m_sram AT > m_text

  .bss (NOLOAD) :
  {
    . = ALIGN(4);
    __BSS_START = .;
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    __BSS_END = .;
  } > m_sram

  .flash_driver_ram (NOLOAD) :
  {
    __FlashDriverStart = .;
    . += LENGTH(m_driver);
    __FlashDriverEnd = .;
  } > m_driver
  __StackTop = ORIGIN(m_stack) + LENGTH(m_stack);
  __StackLimit = __StackTop - STACK_SIZE;

  .stack __StackLimit (NOLOAD) :
  {
    . += STACK_SIZE;
  } > m_stack

  .ARM.attributes 0 : { *(.ARM.attributes) }
  /DISCARD/ : { *(.comment*) *(.eh_frame*) }

  ASSERT(__boot_flash_end__ <= 0x00008000,
         "Bootloader exceeds reserved 32 KB partition")
  ASSERT(__BSS_END <= ORIGIN(m_driver), "Bootloader overlaps RAM driver")
  ASSERT(__FlashDriverEnd <= __StackLimit, "RAM driver overlaps stack")
}
