#include "page.h"
#include <zjunix/utils.h>
#include "arch.h"

#pragma GCC push_options
#pragma GCC optimize("O0")

void init_pgtable() {
    asm volatile(
        "mtc0 $zero, $2\n\t"  //entry_lo0
        "mtc0 $zero, $3\n\t"  //entry_lo1
        "mtc0 $zero, $5\n\t"  //PageMask
        "mtc0 $zero, $6\n\t"  //wired register
        "lui  $t0, 0x8000\n\t"
        "li $t1, 0x2000\n\t"

        "move $v0, $zero\n\t"
        "li $v1, 32\n"

        "init_pgtable_L1:\n\t"
        "mtc0 $v0, $0\n\t"
        "mtc0 $t0, $10\n\t"
        "addu $t0, $t0, $t1\n\t"
        "addi $v0, $v0, 1\n\t"
        "bne $v0, $v1, init_pgtable_L1\n\t"
        "tlbwi\n\t"
        "nop");
}

#pragma GCC pop_options