/* ld script to make m68k Linux kernel */
OUTPUT_FORMAT("elf32-m68k", "elf32-m68k", "elf32-m68k")
OUTPUT_ARCH(m68k)
ENTRY(_start)
MEMORY 
	{
	romvec : ORIGIN = 0x10c00000, LENGTH = 0x10400
	flash  : ORIGIN = 0x10c10400, LENGTH = 0xfec00
	eflash : ORIGIN = 0x10d00000, LENGTH = 1
	ramvec : ORIGIN = 0x00000000, LENGTH = 1024
	ram    : ORIGIN = 0x10000400, LENGTH = 0x100000-0x400
	eram   : ORIGIN = 0x10100000, LENGTH = 1
	}

SECTIONS
{
  .romvec :
	{
	  _flashstart = . ;
	  _romvec = . ;
	} > romvec

  _text = .;                    /* Text and read-only data */
  .text :
        {
	  text_start = . ;
          *(.text)
          *(.text.*)
          *(.rodata)
          *(.fixup)		
          *(__ex_table)	
	  _etext = . ;
	
	  __data_rom_start = ALIGN ( 4 ) ;
        } > flash
  _etext = .;                   /* End of text section */
  
  .eflash :
        {
          _flashend = . ;
        } > eflash
	
  .ramvec :
	{
	  _ramvec = . ;
	} > ramvec
	
  .data :
	{
	  __data_start = . ;
          *(.data)
          *(.data.*)	
          *(.setup.init)		
          *(.exitcall.exit)		
          *(.initcall.init)
	  _edata = . ;
	  edata = ALIGN( 0x10 ) ;
        } > ram
	
  .bss  :
        {
	  __bss_start = ALIGN( 0x10 ) ;
	  __data_end = ALIGN( 0x10 ) ;
	  *(.bss)
	  *(COMMON)
	  end = ALIGN( 0x10 ) ;
	  _end = ALIGN( 0x10 ) ;
        } > ram

   . = ALIGN(4096);              /* Init code and data */
     __init_begin = .;
   .text.init : { *(.text.init) }
   .data.init : { *(.data.init) }
   . = ALIGN(16);
     __setup_start = .;
   .setup.init : { *(.setup.init) }
     __setup_end = .;
     __initcall_start = .;
   .initcall.init : { *(.initcall.init) }
     __initcall_end = .;
  . = ALIGN(8192);
     __init_end = .;
     
  .eram :
	{
  	  _ramend = . ;
	} > eram
}
