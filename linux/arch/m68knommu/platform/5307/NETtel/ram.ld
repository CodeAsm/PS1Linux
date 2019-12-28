
MEMORY {
	ram    : ORIGIN = 0x400, LENGTH = 0x400000
}

SECTIONS {

	.text : {
		_stext = . ;
        	*(.text)
			*(.text.exit)
			*(.text.lock)
			*(.exitcall.exit)
		_etext = ALIGN(0x4) ;
	} > ram

	.data BLOCK(0x4) : {
		_sdata = . ;
		__data_start = . ;
			*(.rodata)
        	*(.data)
			*(.data.exit)
		. = ALIGN(0x2000) ;
        	*(.data.init_task)
		. = ALIGN(0x2000) ;
		_edata = ALIGN(0x4) ;
	} > ram

	.init BLOCK(4096) : {
		__init_begin = .;
			*(.text.init)
			*(.data.init)
		. = ALIGN(16);
		__setup_start = .;
			*(.setup.init)
		__setup_end = .;
		__initcall_start = .;
			*(.initcall.init)
		__initcall_end = ALIGN(4);
		__init_end = .;
	} > ram

	.bss BLOCK(0x4) : {
		_sbss = . ;
			*(.bss)
			*(COMMON)
		_ebss = ALIGN(0x4) ;
		_end = ALIGN(0x4) ;
	} > ram
}

