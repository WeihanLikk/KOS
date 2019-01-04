#include "page.h"
#include <kos/utils.h>
#include <kos/pc/sched.h>
#include "arch.h"

#pragma GCC push_options
#pragma GCC optimize( "O0" )

/* 页表初始化主要是对TLB进行初始化 */
void init_pgtable()
{
	asm volatile(
	  "mtc0 $zero, $2\n\t"   //entry_lo0
	  "mtc0 $zero, $3\n\t"   //entry_lo1
	  "mtc0 $zero, $5\n\t"   //PageMask
	  "mtc0 $zero, $6\n\t"   //wired register ???
	  "mtc0 $zero, $10\n\t"  //EntryHi

	  //"lui  $t0, 0x8000\n\t"
	  //"li $t1, 0x2000\n\t"

	  "move $v0, $zero\n\t"  //$2($v0)用于子程序的非浮点结果或返回值,set to 0
	  "li $v1, 32\n"		 //$3($v1)用于子程序的非浮点结果或返回值,set to 32

	  "init_pgtable_L1:\n\t"
	  "mtc0 $v0, $0\n\t"
	  // "mtc0 $t0, $10\n\t"
	  // "addu $t0, $t0, $t1\n\t"
	  "addi $v0, $v0, 1\n\t"
	  "bne $v0, $v1, init_pgtable_L1\n\t"  // index = 31
	  "tlbwi\n\t"						   // write Tlb entry at index
	  "nop" );
}

PageTableEntry set_pgtable_entry( unsigned int pgd_v )
{
	unsigned int pgd_h;
	struct task_struct *current_task = my_cfs_rq.current_task;
	PageTableEntry pte;
	pgd_h = pgd_v >> 12;
	pgd_h &= 0x7fff;
	pte.EntryHi.VPN2 = pgd_h;
	pte.EntryHi.ASID = current_task->ASID;
	pte.EntryHi.reserved = 0;
	pte.EntryLo0.PFN = pte.EntryLo1.PFN = 0;
	pte.EntryLo0.V = pte.EntryLo1.V = 0;
	pte.EntryLo0.G = pte.EntryLo1.G = 0;
	pte.EntryLo0.C = pte.EntryLo1.C = 0;
	pte.EntryLo0.D = pte.EntryLo1.D = 1;
	pte.PageMask.Mask = 0xff;
}

#pragma GCC pop_options