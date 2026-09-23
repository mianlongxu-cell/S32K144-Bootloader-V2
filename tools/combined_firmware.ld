ENTRY(Reset_Handler)

SECTIONS
{
  .boot 0x00000000 :
  {
    Reset_Handler = 0x00000411;
    KEEP(*(.boot))
  }

  .app 0x00008000 :
  {
    KEEP(*(.app))
  }

  .metadata 0x0007F000 :
  {
    KEEP(*(.metadata))
  }

  /DISCARD/ : { *(.comment*) *(.ARM.attributes*) }
}
