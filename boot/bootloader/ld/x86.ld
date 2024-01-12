OUTPUT_FORMAT("binary")

SECTIONS
{
	. = 0x8000;	
	
	.text BLOCK(4096) : ALIGN(4096)
	{
		*(.entrypoint)
		*(.text)
		*(.rodata)
	}
	
	.data BLOCK(4096) : ALIGN(4096)
	{
		*(.data)
	}

	.lockedtext BLOCK(4096) : ALIGN(4096)
	{
		*(.lockedtext)
		*(.lockedrodata)
	}
	
	.lockeddata BLOCK(4096) : ALIGN(4096)
	{
		*(.lockeddata)
	}
	
	.bss BLOCK(4096) : ALIGN(4096)
	{
		*(COMMON)
		*(.bss)
		*(.bootstrap_stack)
	}
		
	/DISCARD/ :
    {
        *(.comment)
    }
}