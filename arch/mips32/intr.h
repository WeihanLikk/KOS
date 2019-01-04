#ifndef _INTR_H
#define _INTR_H

#include <kos/pc/sched.h>

typedef void ( *intr_fn )( unsigned int, unsigned int, struct reg_context *context );

extern intr_fn interrupts[ 8 ];

void init_interrupts();
int enable_interrupts();
int disable_interrupts();
void do_interrupts( unsigned int status, unsigned int cause, struct reg_context *pt_context );
void register_interrupt_handler( int index, intr_fn fn );

#endif