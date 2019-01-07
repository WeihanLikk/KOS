#ifndef _EXC_H
#define _EXC_H

#include <kos/pc/sched.h>

typedef void ( *exc_fn )( unsigned int, unsigned int, context * );

extern exc_fn exceptions[ 32 ];
extern int exl;

void do_exceptions( unsigned int status, unsigned int cause, context *pt_context, unsigned int bad_addr );
void register_exception_handler( int index, exc_fn fn );
void init_exception();
void do_tlb_refill( unsigned int status, unsigned int cause, context *pt_context, unsigned int badVaddr );

#endif