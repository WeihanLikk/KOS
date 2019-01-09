#include "exc.h"

#include <driver/vga.h>
#include <page.h>
#include <kos/vm/vmm.h>
#include <kos/pc/sched.h>
#include <kos/utils.h>
#include <kos/slab.h>
#include <kos/page.h>
#include <driver/ps2.h>

/* The only general purpose registers allowed in TLB handlers. */
#define K0 26
#define K1 27

/* Some CP0 registers */
#define C0_INDEX 0, 0
#define C0_ENTRYLO0 2, 0
#define C0_TCBIND 2, 2
#define C0_ENTRYLO1 3, 0
#define C0_CONTEXT 4, 0
#define C0_PAGEMASK 5, 0
#define C0_BADVADDR 8, 0
#define C0_ENTRYHI 10, 0
#define C0_EPC 14, 0
#define C0_XCONTEXT 20, 0

#pragma GCC push_options
#pragma GCC optimize( "O0" )

static unsigned int tlb_handler[ 128 ];
exc_fn exceptions[ 32 ];
int count = 0;
int count_2 = 0;
int exl;

void do_tlb_refill( unsigned int status, unsigned int cause, context *pt_context, unsigned int badVaddr )
{
	kernel_printf( "In do_tlbrefill: badVaddr:%x\n", badVaddr );
	if ( badVaddr >= 0x80000000 )
	{
#ifdef TLB_DEBUG
		kernel_printf( "Invalid Address!!!badVaddr:%x\n", badVaddr );
#endif  //TLB_DEBUG
		while ( 1 )
			;
	}
	//if(badVaddr is not in vma){}
	kernel_printf( "status:%x\n", status );
	int index = status >> 1;
	index &= 0x1;
	kernel_printf( "exl:%x\n", exl );
	if ( exl == 1 )
	{
//exl == 1, inside an exception(tlb_refill)
#ifdef TLB_DEBUG
		kernel_printf( "exl=1, inside an exception(tlb_refill),badVaddr:%x\n", badVaddr );
#endif														//TLB_DEBUG
		exceptions[ 7 & 31 ]( status, cause, pt_context );  // tlb_load
		return;
	}
	else if ( exl )
	{
#ifdef TLB_DEBUG
		kernel_printf( "Error!!!badVaddr:%x\n", badVaddr );
#endif  //TLB_DEBUG
		while ( 1 )
			;
	}
	exl++;  //change exl = 1, represent that now we are in an excpetion
			// if there nots exist a process
#ifdef TLB_DEBUG
	kernel_printf( "current_task:%x,current_task->pid:%x\n", current_task, current_task->pid );
#endif  //TLB_DEBUG
	if ( current_task->pid == 0 )
	{
#ifdef TLB_DEBUG
		kernel_printf( "current_task is not a user process\n" );
#endif  //TLB_DEBUG
		unsigned int paddr, pfn, vpn;
		unsigned int entry_lo0, entry_lo1;
		unsigned int entry_hi, pagemask;
		//fill_vaddr_into_tlb
		paddr = (unsigned int)alloc_pages( 1 );
		if ( !paddr )
		{
#ifdef TLB_DEBUG
			kernel_printf( "physical page alloc fail!\n" );
			while ( 1 )
				;
#endif  //TLB_DEBUG
		}
		pfn = paddr >> PAGE_SHIFT;
		vpn = badVaddr >> PAGE_SHIFT;
		// set acid = 0 for no process
		entry_hi = vpn << 13;
		entry_lo0 = pfn << 6;
		// set v = 1
		entry_lo0 += 2;
		entry_lo1 = 0;
		pagemask = 0x8ff;
#ifdef TLB_DEBUG
		kernel_printf( "vpn: %x, pfn: %x, entry_hi: %x, entry_lo0: %x, entry_lo1: %x, pagemask:%x\n", vpn, pfn, entry_hi, entry_lo0, entry_lo1, pagemask );
#endif
		unsigned int context_cp0;
		asm volatile(
		  "mfc0 %0, $4\n\t"   //context, page table entry
		  "nop\n\t"			  //CP0 hazard
		  "mtc0 %1, $10\n\t"  //context, page table entry
		  "nop\n\t"			  //CP0 hazard
		  : "=r"( context_cp0 )
		  : "r"( entry_hi ) );
		asm volatile(
		  "mfc0 %0, $4\n\t"
		  "nop\n\t"			 //CP0 hazard
		  "mtc0 %1, $2\n\t"  //context, page table entry
		  "nop\n\t"			 //CP0 hazard
		  : "=r"( context_cp0 )
		  : "r"( entry_lo0 ) );
		asm volatile(
		  "mfc0 %0, $4\n\t"
		  "nop\n\t"			 //CP0 hazard
		  "mtc0 %1, $3\n\t"  //context, page table entry
		  "nop\n\t"			 //CP0 hazard
		  : "=r"( context_cp0 )
		  : "r"( entry_lo1 ) );
		asm volatile(
		  "mfc0 %0, $4\n\t"
		  "nop\n\t"			 //CP0 hazard
		  "mtc0 %1, $5\n\t"  //context, page table entry
		  "nop\n\t"			 //CP0 hazard
		  : "=r"( context_cp0 )
		  : "r"( pagemask ) );
		asm volatile(
		  "tlbwr\n\t"
		  "nop\n\t"
		  "nop\n\t" );
		// when finish change exl = 0
		exl = 0;
		return;
	}
	//在page table中查找触发tlb_refill异常的bad address
	/* 页目录 */
	PageTableEntry *pgd = current_task->mm->pgd;
#ifdef TLB_DEBUG
	kernel_printf( "pgd:%x,vbadVaddr:%x\n", pgd, badVaddr );
#endif  //TLB_DEBUG
	unsigned int pgdc = (unsigned int)pgd;
	unsigned int *p;
	unsigned int vpn = badVaddr >> 12;
	vpn &= 0x1ffff;  //19 bits
	unsigned int ptec = pgdc + vpn * sizeof( PageTableEntry );
	PageTableEntry *pte = (PageTableEntry *)ptec;
	if ( !pte->EntryLo0.PFN )
	{
		// _do_page_fault(pte,vpn);
		return;
	}
	// when finish change exl = 0
	exl = 0;
}

// void do_tlbrefill(unsigned int status, unsigned int cause, context *pt_context,  unsigned int bad_addr )
// {
// 	pgd_t *pgd;
// 	unsigned int pde_index, pte_index, pte_index_near;
// 	unsigned int pde, pte, pte_near;
// 	unsigned int pte_phy, pte_near_phy;
// 	unsigned int *pde_ptr, pte_ptr;
// 	unsigned int entry_lo0, entry_lo1;
// 	unsigned int entry_hi;

// #ifdef TLB_DEBUG
// 	unsigned int entry_hi_test;
// 	asm volatile(
// 	  "mfc0  $t0, $10\n\t"
// 	  "move  %0, $t0\n\t"
// 	  : "=r"( entry_hi_test ) );

// 	kernel_printf( "tlb_refill: bad_addr = %x    entry_hi = %x \n", bad_addr, entry_hi_test );
// 	kernel_printf( "%x  %d\n", current_task, current_task->pid );
// #endif
// 	if ( current_task->mm == 0 )
// 	{
// 		kernel_printf( "tlb_refill: mm is null!!!  %d\n", current_task->pid );
// 		goto error_0;
// 	}

// 	pgd = current_task->mm->pgd;
// 	if ( pgd == 0 )
// 	{
// 		kernel_printf( "tlb_refill: pgd == NULL\n" );
// 		goto error_0;
// 	}

// 	bad_addr &= PAGE_MASK;

// 	//搜索bad_addr是否在vma中,如果不在任何vma中，报错
// 	//.......
// 	//To be done

// 	pde_index = bad_addr >> PGD_SHIFT;
// 	pde = pgd[ pde_index ];
// 	pde &= PAGE_MASK;
// 	if ( pde == 0 )
// 	{  //二级页表不存在
// 		pde = (unsigned int)kmalloc( PAGE_SIZE );

// #ifdef TLB_DEBUG
// 		kernel_printf( "second page table not exist\n" );
// #endif

// 		if ( pde == 0 )
// 		{
// 			kernel_printf( "tlb_refill: alloc second page table failed!\n" );
// 			goto error_0;
// 		}

// 		kernel_memset( (void *)pde, 0, PAGE_SIZE );
// 		pgd[ pde_index ] = pde;
// 		pgd[ pde_index ] &= PAGE_MASK;
// 		pgd[ pde_index ] |= 0x0f;  //attr
// 	}

// #ifdef VMA_DEBUG
// 	kernel_printf( "tlb refill: %x\n", pde );
// #endif
// 	pde_ptr = (unsigned int *)pde;
// 	pte_index = ( bad_addr >> PAGE_SHIFT ) & INDEX_MASK;
// 	pte = pde_ptr[ pte_index ];
// 	pte &= PAGE_MASK;
// 	if ( pte == 0 )
// 	{
// #ifdef TLB_DEBUG
// 		kernel_printf( "page not exist\n" );
// #endif

// 		pte = (unsigned int)kmalloc( PAGE_SIZE );  //要考虑物理地址？？？

// 		if ( pte == 0 )
// 		{
// 			kernel_printf( "tlb_refill: alloc page failed!\n" );
// 			goto error_0;
// 		}

// 		kernel_memset( (void *)pte, 0, PAGE_SIZE );
// 		pde_ptr[ pte_index ] = pte;
// 		pde_ptr[ pte_index ] &= PAGE_MASK;
// 		pde_ptr[ pte_index ] |= 0x0f;
// 	}

// 	pte_index_near = pte_index ^ 0x01;
// 	pte_near = pde_ptr[ pte_index_near ];
// 	pte_near &= PAGE_MASK;

// #ifdef VMA_DEBUG
// 	kernel_printf( "pte: %x pte_index: %x  pte_near_index: %x\n", pte, pte_index, pte_index_near );
// #endif

// 	if ( pte_near == 0 )
// 	{  //附近项 为空
// #ifdef TLB_DEBUG
// 		kernel_printf( "page near not exist\n" );
// #endif

// 		pte_near = (unsigned int)kmalloc( PAGE_SIZE );

// 		if ( pte_near == 0 )
// 		{
// 			kernel_printf( "tlb_refill: alloc pte_near failed!\n" );
// 			goto error_0;
// 		}

// 		kernel_memset( (void *)pte_near, 0, PAGE_SIZE );
// 		pde_ptr[ pte_index_near ] = pte_near;
// 		pde_ptr[ pte_index_near ] &= PAGE_MASK;
// 		pde_ptr[ pte_index_near ] |= 0x0f;
// 	}

// 	//换成物理地址
// 	pte_phy = pte - 0x80000000;
// 	pte_near_phy = pte_near - 0x80000000;
// #ifdef TLB_DEBUG
// 	kernel_printf( "pte: %x  %x\n", pte_phy, pte_near_phy );
// #endif
// 	//
// 	if ( pte_index & 0x01 == 0 )
// 	{  //偶
// 		entry_lo0 = ( pte_phy >> 12 ) << 6;
// 		entry_lo1 = ( pte_near_phy >> 12 ) << 6;
// 	}
// 	else
// 	{
// 		entry_lo0 = ( pte_near_phy >> 12 ) << 6;
// 		entry_lo1 = ( pte_near >> 12 ) << 6;
// 	}
// 	entry_lo0 |= ( 3 << 3 );  //cached ??
// 	entry_lo1 |= ( 3 << 3 );  //cached ??
// 	entry_lo0 |= 0x06;		  //D = 1, V = 1, G = 0
// 	entry_lo1 |= 0x06;

// 	entry_hi = ( bad_addr & PAGE_MASK ) & ( ~( 1 << PAGE_SHIFT ) );
// 	entry_hi |= current_task->ASID;

// #ifdef TLB_DEBUG
// 	kernel_printf( "pid: %d\n", current_task->pid );
// 	kernel_printf( "tlb_refill: entry_hi: %x  entry_lo0: %x  entry_lo1: %x\n", entry_hi, entry_lo0, entry_lo1 );
// #endif

// 	asm volatile(
// 	  "move $t0, %0\n\t"
// 	  "move $t1, %1\n\t"
// 	  "move $t2, %2\n\t"
// 	  "mtc0 $t0, $10\n\t"
// 	  "mtc0 $zero, $5\n\t"
// 	  "mtc0 $t1, $2\n\t"
// 	  "mtc0 $t2, $3\n\t"
// 	  "nop\n\t"
// 	  "nop\n\t"
// 	  "tlbwr\n\t"
// 	  "nop\n\t"
// 	  "nop\n\t"
// 	  :
// 	  : "r"( entry_hi ),
// 		"r"( entry_lo0 ),
// 		"r"( entry_lo1 ) );

// 	kernel_printf( "after refill\n" );
// 	unsigned int *pgd_ = current_task->mm->pgd;
// 	unsigned int pde_, pte_;
// 	unsigned int *pde_ptr_;
// 	int i, j;
// 	count_2++;

// 	for ( i = 0; i < 1024; i++ )
// 	{
// 		pde_ = pgd_[ i ];
// 		pde_ &= PAGE_MASK;

// 		if ( pde_ == 0 )  //不存在二级页表
// 			continue;
// 		kernel_printf( "pde: %x\n", pde_ );
// 		pde_ptr_ = (unsigned int *)pde_;
// 		for ( j = 0; j < 1024; j++ )
// 		{
// 			pte_ = pde_ptr_[ j ];
// 			pte_ &= PAGE_MASK;
// 			if ( pte_ != 0 )
// 			{
// 				kernel_printf( "\tpte: %x\n", pte_ );
// 			}
// 		}
// 	}
// 	// if (count_2 == 4) {
// 	//     kernel_printf("")
// 	//     while(1)
// 	//         ;
// 	// }

// 	return;

// error_0:
// 	while ( 1 )
// 		;
// }

void do_exceptions( unsigned int status, unsigned int cause, context *pt_context, unsigned int bad_addr )
{
	int index = cause >> 2;
	index &= 0x1f;
	if ( bad_addr < 0x80000000 )
	{
		//do_tlb_refill(status,cause,pt_context, bad_addr );
#ifdef TLB_DEBUG
		kernel_printf( "badVaddr: %x, refill done\n", bad_addr );

		//count = 0x
		// kernel_getchar();
#endif
		return;
	}
#ifdef TLB_DEBUG
	unsigned int count;
#endif

	//return;

	if ( exceptions[ index ] )
	{
		exceptions[ index ]( status, cause, pt_context );
	}
	else
	{
		struct task_struct *pcb;
		unsigned int badVaddr;
		asm volatile( "mfc0 %0, $8\n\t"
					  : "=r"( badVaddr ) );
		//modified by Ice
		pcb = current_task;
		kernel_printf( "\nProcess %s exited due to exception cause=%x;\n", pcb->name, cause );
		kernel_printf( "status=%x, EPC=%x, BadVaddr=%x\n", status, pcb->context.epc, badVaddr );
		//    pc_kill_syscall(status, cause, pt_context);
		//Done by Ice
		while ( 1 )
			;
	}
}

void register_exception_handler( int index, exc_fn fn )
{
	index &= 31;
	exceptions[ index ] = fn;
}

void init_exception()
{
	// status 0000 0000 0000 0000 0000 0000 0000 0000
	// cause 0000 0000 1000 0000 0000 0000 0000 0000
	asm volatile(
	  "mtc0 $zero, $12\n\t"
	  "li $t0, 0x800000\n\t"
	  "mtc0 $t0, $13\n\t" );
	exl = 0;
}

#pragma GCC pop_options