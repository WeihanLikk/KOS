
#include <arch.h>
#include <kos/syscall.h>

void syscall4( unsigned int status, unsigned int cause, struct reg_context *pt_context )
{
	kernel_puts( (unsigned char *)pt_context->a0, 0xfff, 0 );
}