ENTRY(_start)
OUTPUT_FORMAT("elf32-i386")

SECTIONS
{
	. = 1M;
	.lowram.text : {
		*(.lowram.text)
	}
	
	. += 0xC0000000;
	
	.text ALIGN(4096) : AT (ADDR (.text) - 0xC0000000)
	{
		*(.text .text.*)
	
		. = ALIGN(8);
		
		start_ctors = .;
        *(SORT(.ctors*))  	/* Note the "SORT" */
        end_ctors = .;

		*(.dtors)
	}
	
	.rodata ALIGN(4096) : AT (ADDR (.rodata) - 0xC0000000)
	{
		*(.rodata)
	}


	. = ALIGN(4096);
	_start_pageablek_section = .;

	.pageablek ALIGN(4096) : AT (ADDR (.data) + SIZEOF(.data) - 0xC0000000)
	{
		*(.pageablektext)
		*(.pageablekdata)
	}
	_end_pageablek_section = .;

	.data ALIGN(4096) : AT (ADDR (.data) - 0xC0000000)
	{
		*(.data)
	}

	.bss ALIGN(4096) : AT (ADDR (.bss) - 0xC0000000)
	{
		*(COMMON)
		*(.bss)
		*(.bootstrap_stack)
	}
	
	_kernel_end = .;
	
	/DISCARD/ :
    {
        *(.comment)
    }
}