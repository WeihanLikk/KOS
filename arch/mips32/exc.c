#include "exc.h"

#include <driver/vga.h>
#include <page.h>
#include <zjunix/pc/sched.h>
#include <zjunix/utils.h>
#include <zjunix/slab.h>
#include <zjunix/page.h>
#include <driver/ps2.h>
#include <arch.h>

#pragma GCC push_options
#pragma GCC optimize( "O0" )

exc_fn exceptions[ 32 ];
int count = 0;
int exl = 0;

void tlb_refill( unsigned int status, unsigned int cause, context *pt_context, unsigned int bad_vaddr )
{
	int index = cause >> 2;
	index &= 0x1f;
#ifdef TLB_DEBUG
//kernel_printf("badVaddr:%x cause:%x status:%x\n", bad_vaddr, cause, status);
#endif
	kernel_printf( "badVaddr:%x cause:%x status:%x\n", bad_vaddr, cause, status );
	if ( index != 3 )
	{
		//make sure not caused by process exit
		return;
	}
	unsigned int *pgd;
	unsigned int pde_index, pte_index, pte_index_near;
	unsigned int pde, pte, pte_near;
	unsigned int pte_phy, pte_near_phy;
	unsigned int *pde_ptr, pte_ptr;
	unsigned int entry_lo0, entry_lo1;
	unsigned int entry_hi;

	int has_vma = 1;
	if ( bad_vaddr >= 0x80000000 )
	{
		kernel_printf( "In do_tlb_refill: Invalid Address!!!badVaddr:%x\n", bad_vaddr );
		goto error_0;
	}

	if ( my_cfs_rq.current_task->mm == 0 )
	{
		kernel_printf( "tlb_refill: mm is null!!!  %d\n", my_cfs_rq.current_task->pid );
		goto error_0;
	}

	if ( exl == 1 )
	{
//exl == 1, inside an exception(tlb_refill)
#ifdef TLB_DEBUG
		kernel_printf( "exl=1, inside an exception(tlb_refill),badVaddr:%x\n", bad_vaddr );
#endif  //TLB_DEBUG \
  //tlb_load( status, cause, pt_context, bad_vaddr );  // tlb_load
		return;
	}
	else if ( exl )
	{
#ifdef TLB_DEBUG
		kernel_printf( "In do_tlb_refill: EXL Error!!!badVaddr:%x\n", bad_vaddr );
#endif  //TLB_DEBUG \
		//goto error_0;
	}

	pgd = (unsigned int *)my_cfs_rq.current_task->mm->pgd;
	if ( pgd == 0 )
	{
		kernel_printf( "tlb_refill: pgd == NULL\n" );
		goto error_0;
	}

	bad_vaddr &= PAGE_MASK;

	if ( my_cfs_rq.current_task->mm->map_count == 0 )
	{
#ifdef TLB_DEBUG
		kernel_printf( "In do_tlb_refill: no vma! pid: %d\n", current_task->pid );
#endif  //TLB_DEBUG
		has_vma = 0;
		goto judge;
	}
	//int flag = is_in_vma(bad_vaddr);
	int flag = 1;
	if ( !flag )
	{
		kernel_printf( "Error: badVaddr is not in vma! pid: %d\n", my_cfs_rq.current_task->pid );
		goto error_0;
	}
judge:
	exl++;  //change exl = 1, represent that now we are in an excpetion
	pde_index = bad_vaddr >> PGD_SHIFT;
	pde = pgd[ pde_index ];  //will cause another tlb refill
	//kernel_printf( "here, should print after tlbload\n");
	pde &= PAGE_MASK;
	if ( pde == 0 )
	{  //二级页表不存在, will cause another tlb refill

		pde = (unsigned int)kmalloc( PAGE_SIZE );

		if ( pde == 0 )
		{
			kernel_printf( "tlb_refill: alloc second page table failed!\n" );
			goto error_0;
		}
#ifdef TLB_DEBUG
		kernel_printf( "create pde: %x\n", pde );
#endif

		kernel_memset( (void *)pde, 0, PAGE_SIZE );  //this page to be pde***
		pgd[ pde_index ] = pde;
		pgd[ pde_index ] &= PAGE_MASK;
		pgd[ pde_index ] |= 0x0f;  //attr
	}

	pde_ptr = (unsigned int *)pde;
	pte_index = ( bad_vaddr >> PAGE_SHIFT ) & INDEX_MASK;
	pte = pde_ptr[ pte_index ];
	pte &= PAGE_MASK;
	if ( pte == 0 )
	{  //页表不存在, will cause another tlb refill

		pte = (unsigned int)kmalloc( PAGE_SIZE );  //要考虑物理地址？？？
		if ( pte == 0 )
		{
			kernel_printf( "tlb_refill: alloc page failed!\n" );
			goto error_0;
		}
#ifdef TLB_DEBUG
		kernel_printf( "alloc physic addr: %x;\n", pte );
#endif

		kernel_memset( (void *)pte, 0, PAGE_SIZE );
		pde_ptr[ pte_index ] = pte;
		pde_ptr[ pte_index ] &= PAGE_MASK;
		pde_ptr[ pte_index ] |= 0x0f;
	}

	pte_index_near = pte_index ^ 0x01;
	pte_near = pde_ptr[ pte_index_near ];
	pte_near &= PAGE_MASK;

	if ( pte_near == 0 )
	{  //附近项 为空
#ifdef TLB_DEBUG
		// kernel_printf("page near not exist\n");
#endif

		pte_near = (unsigned int)kmalloc( PAGE_SIZE );

		if ( pte_near == 0 )
		{
			kernel_printf( "tlb_refill: alloc pte_near failed!\n" );
			goto error_0;
		}
#ifdef TLB_DEBUG
		kernel_printf( " ,alloc next physic addr: %x\n", pte_near );
#endif
		kernel_memset( (void *)pte_near, 0, PAGE_SIZE );
		pde_ptr[ pte_index_near ] = pte_near;
		pde_ptr[ pte_index_near ] &= PAGE_MASK;
		pde_ptr[ pte_index_near ] |= 0x0f;
	}

	//换成物理地址
	pte_phy = pte - 0x80000000;
	pte_near_phy = pte_near - 0x80000000;
	//
	if ( pte_index & 0x01 == 0 )
	{  //偶
		entry_lo0 = ( pte_phy >> 12 ) << 6;
		entry_lo1 = ( pte_near_phy >> 12 ) << 6;
	}
	else
	{
		entry_lo0 = ( pte_near_phy >> 12 ) << 6;
		entry_lo1 = ( pte_near >> 12 ) << 6;
	}
	entry_lo0 |= ( 3 << 3 );  //cached ??
	entry_lo1 |= ( 3 << 3 );  //cached ??
	entry_lo0 |= 0x06;		  //D = 1, V = 1, G = 0
	entry_lo1 |= 0x06;

	entry_hi = ( bad_vaddr & PAGE_MASK ) & ( ~( 1 << PAGE_SHIFT ) );
	entry_hi |= my_cfs_rq.current_task->ASID;

#ifdef TLB_DEBUG
	//   kernel_printf("pid: %d\n", current_task->pid);
	//   kernel_printf("tlb_refill: entry_hi: %x  entry_lo0: %x  entry_lo1: %x\n", entry_hi, entry_lo0, entry_lo1);
#endif

	asm volatile(
	  "move $t0, %0\n\t"
	  "move $t1, %1\n\t"
	  "move $t2, %2\n\t"
	  "mtc0 $t0, $10\n\t"
	  "mtc0 $zero, $5\n\t"
	  "mtc0 $t1, $2\n\t"
	  "mtc0 $t2, $3\n\t"
	  "nop\n\t"
	  "nop\n\t"
	  "tlbwr\n\t"
	  "nop\n\t"
	  "nop\n\t"
	  :
	  : "r"( entry_hi ),
		"r"( entry_lo0 ),
		"r"( entry_lo1 ) );

	kernel_printf( "after refill\n" );
	exl = 0;
	// unsigned int* pgd_ = current_task->mm->pgd;
	// unsigned int pde_, pte_;
	// unsigned int* pde_ptr_;
	// int i, j;
	// count_2 ++;

	// for (i = 0; i < 1024; i++) {
	//     pde_ = pgd_[i];
	//     pde_ &= PAGE_MASK;

	//     if (pde_ == 0)  //不存在二级页表
	//         continue;
	//    // kernel_printf("pde: %x\n", pde_);
	//     pde_ptr_ = (unsigned int*)pde_;
	//     for (j = 0; j < 1024; j++) {
	//         pte_ = pde_ptr_[j];
	//         pte_ &= PAGE_MASK;
	//         if (pte_ != 0) {
	//           //  kernel_printf("\tpte: %x\n", pte_);
	//         }
	//     }
	// }
	// if (count_2 == 4) {
	//     kernel_printf("")
	//     while(1)
	//         ;
	// }

	return;

error_0:
	while ( 1 )
		;
}

void do_exceptions( unsigned int status, unsigned int cause, context *pt_context, unsigned int bad_vaddr )
{
	int index = cause >> 2;
	index &= 0x1f;

#ifdef TLB_DEBUG
	unsigned int count;
#endif

	if ( index == 2 || index == 3 || index == 5 )
	{
//tlb_refill(bad_vaddr);
#ifdef TLB_DEBUG
		//kernel_printf("refill done\n");

		//count = 0x
		// kernel_getchar();
#endif
		return;
	}

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
		pcb = my_cfs_rq.current_task;
		kernel_printf( "\nProcess %s exited due to exception cause=%x;", pcb->pid, cause );
		kernel_printf( "status=%x, EPC=%x, BadVaddr=%x\n", status, pcb->context.epc, badVaddr );
		//    pc_kill_syscall(status, cause, pt_context);
		//Done by Ice
		return;
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
}

#pragma GCC pop_options