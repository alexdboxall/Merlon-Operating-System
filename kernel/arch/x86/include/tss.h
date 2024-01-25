
#pragma once

#include <common.h>

/*
* The task state segment was designed to store information about a task so 
* that task switching could be done in hardware. We do not use it for this purpose,
* instead only using it to set the stack correctly after a user -> kernel switch.
*
* The layout of this structure is mandated by the CPU.
*/ 

struct tss
{
	uint16_t link;			// used
	uint16_t unused_1;
	uint32_t esp0;			// used
	uint16_t ss0;			// used
	uint8_t unused_2[92];
	uint16_t iopb;			// used
	
} __attribute__((packed));

void x86InitTss(void);
