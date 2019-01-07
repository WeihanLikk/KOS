#ifndef _ZJUNIX_TLBLOAD_H
#define _ZJUNIX_TLBLOAD_H


void init_tlbload();
void tlbload(unsigned int status, unsigned int cause, struct reg_context* pt_context);

#endif // ! _ZJUNIX_SYSCALL_H