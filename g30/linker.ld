ENTRY(Reset_Handler)
MEMORY
{
  FLASH (rx)  : ORIGIN = 0x08001000, LENGTH = 52K
  RAM (rwx)   : ORIGIN = 0x20000000, LENGTH = 20K
}
_estack = ORIGIN(RAM) + LENGTH(RAM);
SECTIONS
{
  .isr_vector : { KEEP(*(.isr_vector)) } > FLASH
  .text : { *(.text*) *(.rodata*) } > FLASH
  .ARM.exidx : { *(.ARM.exidx*) } > FLASH
  .data : AT(ADDR(.ARM.exidx) + SIZEOF(.ARM.exidx))
  { _sdata = .; *(.data*) _edata = .; } > RAM
  _sidata = LOADADDR(.data);
  .bss : { _sbss = .; *(.bss*) *(COMMON) _ebss = .; } > RAM
  /DISCARD/ : { *(.comment*) *(.note*) }
}
