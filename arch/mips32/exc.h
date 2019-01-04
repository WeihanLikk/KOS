#ifndef _EXC_H
#define _EXC_H

#include <page.h>
#include <kos/pc/sched.h>
#include <driver/vga.h>
#include <kos/mm/buddy.h>
#include <arch.h>

typedef void ( *exc_fn )( unsigned int, unsigned int, struct reg_context * );

extern exc_fn exceptions[ 32 ];

void do_exceptions( unsigned int status, unsigned int cause, struct reg_context *pt_context );
void register_exception_handler( int index, exc_fn fn );
void init_exception();
void tlb_exception( unsigned int status, unsigned int cause, struct reg_context *pt_context );

#endif