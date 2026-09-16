ENTRY(Reset_Handler)
MEMORY { RAM (rwx) : ORIGIN = 0x00000000, LENGTH = 4M
         RAM2 (rwx): ORIGIN = 0x20000000, LENGTH = 4M }
SECTIONS
{
  .isr_vector : { KEEP(*(.isr_vector)) } > RAM
  .text : { *(.text*) KEEP(*(.init)) KEEP(*(.fini)) *(.rodata*) } > RAM
  .ARM.exidx : { __exidx_start = .; *(.ARM.exidx*) __exidx_end = .; } > RAM
  .init_array : { __init_array_start = .; KEEP(*(.init_array*)) __init_array_end = .; } > RAM
  .fini_array : { __fini_array_start = .; KEEP(*(.fini_array*)) __fini_array_end = .; } > RAM
  .preinit_array : { __preinit_array_start = .; KEEP(*(.preinit_array*)) __preinit_array_end = .; } > RAM
  .data : { *(.data*) } > RAM
  .bss : { __bss_start__ = .; *(.bss*) *(COMMON) __bss_end__ = .; } > RAM
  end = .; _end = .; __end__ = .;
  __stack = ORIGIN(RAM2) + LENGTH(RAM2);
  __heap_limit = ORIGIN(RAM2) + LENGTH(RAM2) - 0x10000;
}
