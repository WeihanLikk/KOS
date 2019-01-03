#ifndef _SYSCALL4_H
#define _SYSCALL4_H

void syscall4( unsigned int status, unsigned int cause, struct reg_context *pt_context );

#endif  // ! _SYSCALL4_H