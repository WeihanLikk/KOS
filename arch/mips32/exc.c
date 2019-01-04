#include "exc.h"

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

static unsigned int tlb_handler[ 128 ];

#pragma GCC push_options
#pragma GCC optimize( "O0" )

exc_fn exceptions[ 32 ];

static void _do_page_fault( PageTableEntry *pte, unsigned int vpn )
{
#ifdef TLB_DEBUG
	kernel_printf( "Begin to do page fault...\n" );
#endif  //TLB_DEBUG
	unsigned int phy_page = (unsigned int)alloc_pages( 0 );
	if ( !phy_page )
	{
#ifdef TLB_DEBUG
		kernel_printf( "fail to malloc pysical memory through buddy...\n" );
#endif  //TLB_DEBUG
	}
	unsigned int pfn = phy_page >> 12;
#ifdef TLB_DEBUG
	kernel_printf( "sucessfully get physical address.\n" );
	kernel_printf( "Vaddr:%x, pfn:%x\n", badVaddr, pfn );
#endif  //TLB_DEBUG
	/* write_page_table(pte, pfn)*/
	//unsigned int acid = current_task->ASID;
	unsigned int acid = my_cfs_rq.current_task->ASID;
	pte->EntryLo0.PFN = pfn;
	pte->EntryLo0.V = 1;
	pte->EntryHi.VPN2 = vpn;
	pte->EntryHi.ASID = acid;
#ifdef TLB_DEBUG
	kernel_printf( "sucessfully write_page_table(acid,vpn,pfn,1).\n" );
#endif  //TLB_DEBUG
}

void tlb_exception( unsigned int status, unsigned int cause, struct reg_context *pt_context )
{
	unsigned int badVaddr;
	asm volatile(
	  "mfc0  %0, $8\n\t"  //load from cp0$8
	  : "=r"( badVaddr ) );
	if ( badVaddr >= KERNEL_ENTRY )
	{
#ifdef TLB_DEBUG
		kernel_printf( "Invalid Address!!!badVaddr:%x\n", badVaddr );
#endif  //TLB_DEBUG
		while ( 1 )
			;
	}
	int index = status >> 1;
	index &= 0x1;
	if ( index )
	{														//index == 1, inside an exception(tlb_refill)
		exceptions[ 7 & 31 ]( status, cause, pt_context );  // tlb_load
		return;
	}
	/* 页目录 */
	pgd_t *pgd = my_cfs_rq.current_task->mm->pgd;
	unsigned int pgdc = (unsigned int)pgd;
	unsigned int *p;
	unsigned int vpn = badVaddr >> 12;
	vpn &= 0x1ffff;  //19 bits
	unsigned int ptec = pgdc + vpn * sizeof( PageTableEntry );
	pte_t *pte = (pte_t *)ptec;
	if ( !pte->EntryLo0.PFN )
	{
		_do_page_fault( pte, vpn );
		return;
	}
#ifdef TLB_DEBUG
	kernel_printf( "Begin to load pte from page table to tlb...\n" );
	kernel_printf( "vpn:%x, pte's virtualAddr:%x\n", vpn, ptec );
	kernel_printf( "acid:%x,vpn:%x,pfn:%x\n", pte->EntryHi.ASID, pte->EntryHi.VPN2, pte->EntryLo0.PFN );
#endif  //TLB_DEBUG                                                                                      \
		// asm volatile(                                                                                 \
		//     "mtc0 %0, $4\n\t" //context, page table entry                                             \
		//     "nop\n\t" //CP0 hazard                                                                    \
		//     "nop\n\t" //CP0 hazard                                                                    \
		//     "mfc0 $k0, $4\n\t" //context, page table entry                                            \
		//     "nop\n\t" //CP0 hazard                                                                    \
		//     "nop\n\t" //CP0 hazard                                                                    \
		//     "lw $k1, 0($k0)\n\t"                                                                      \
		//     "nop\n\t" //CP0 hazard                                                                    \
		//     "nop\n\t" //CP0 hazard                                                                    \
		//     "mtc0 $k1, $2\n\t" //copy the first content in page table entry address to EntryLo0       \
		//     "nop\n\t" //CP0 hazard                                                                    \
		//     "nop\n\t" //CP0 hazard                                                                    \
		//     "lw $k1, 4($k0)\n\t"                                                                      \
		//     "nop\n\t" //CP0 hazard                                                                    \
		//     "nop\n\t" //CP0 hazard                                                                    \
		//     "mtc0 $k1, $3\n\t" //copy the second content in (page table entry address +4) to EntryLo1 \
		//     "nop\n\t" //CP0 hazard                                                                    \
		//     "nop\n\t" //CP0 hazard                                                                    \
		//     "lw $k1, 8($k0)\n\t"                                                                      \
		//     "nop\n\t" //CP0 hazard                                                                    \
		//     "nop\n\t" //CP0 hazard                                                                    \
		//     "mtc0 $k1, $10\n\t" //copy the third content in (page table entry address +8) to EntryHi  \
		//     "nop\n\t" //CP0 hazard                                                                    \
		//     "nop\n\t" //CP0 hazard                                                                    \
		//     "lw $k1, 12($k0)\n\t"                                                                     \
		//     "nop\n\t" //CP0 hazard                                                                    \
		//     "nop\n\t" //CP0 hazard                                                                    \
		//     "mtc0 $k1, $5\n\t" //copy the forth content in (page table entry address +12) to PageMask \
		//     "nop\n\t" //CP0 hazard                                                                    \
		//     "nop\n\t" //CP0 hazard                                                                    \
		//     "tlbwr\n\t"  //write tlb entry selected by random                                         \
		//     :"r"(ptec));
}

void do_exceptions( unsigned int status, unsigned int cause, struct reg_context *pt_context )
{
	int index = cause >> 2;
	index &= 0x1f;
	if ( exceptions[ index ] )
	{
		exceptions[ index ]( status, cause, pt_context );
	}
	else
	{
		struct task_struct *pcb = my_cfs_rq.current_task;
		unsigned int badVaddr;
		asm volatile( "mfc0 %0, $8\n\t"
					  : "=r"( badVaddr ) );
		// pcb = get_curr_pcb();
		//kernel_printf( "Here !!!!\n" );
		kernel_printf( "\nProcess %s exited due to exception cause=%x;\n", pcb->name, cause );
		kernel_printf( "status=%x, EPC=%x, BadVaddr=%x\n", status, pcb->context.epc, badVaddr );
		//pc_kill_syscall(status, cause, pt_context);
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
}

#pragma GCC pop_options