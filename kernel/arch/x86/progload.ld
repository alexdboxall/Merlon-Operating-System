ENTRY(_start)
OUTPUT_FORMAT("binary")

SECTIONS
{
	. = 0xFF800000;
	
	.text ALIGN(4096) : AT (ADDR (.text))
	{
		*(.text)
		*(.rodata)
		*(.symtab)
		*(.strtab)
	}
	
	.data ALIGN(4096) : AT (ADDR (.data))
	{
		*(.data)
	}
	
	.bss ALIGN(4096) : AT (ADDR (.bss))
	{
		*(COMMON)
		*(.bss)
		*(.bootstrap_stack)
	}

	.fake : { . = . + SIZEOF(.bss); }
		
	/DISCARD/ :
    {
        *(.comment)
    }
}