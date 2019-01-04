#include <exc.h>
#include <arch.h>
#include <kos/tlbload.h>
#include <kos/mm/buddy.h>

void init_tlbload()
{
	register_exception_handler( 7, tlbload );
}

void tlbload( unsigned int status, unsigned int cause, struct reg_context *pt_context )
{
	unsigned int badVaddr;
	asm volatile(
	  "mfc0  %0, $8\n\t"  //load from cp0$8
	  : "=r"( badVaddr ) );
	if ( badVaddr >= KERNEL_ENTRY )
	{
#ifdef TLB_DEBUG
		kernel_printf( "In tlbload: Invalid Address!!! badVaddr(pte):%x \n", badVaddr );
#endif  //TLB_DEBUG
		return;
	}
#ifdef TLB_DEBUG
	kernel_printf( "In tlbload: Begin to do page fault...\n" );
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
	/* write_page_table*/
	struct cfs_rq *cfs_rq = &my_cfs_rq;
	struct task_struct *current_task = cfs_rq->current_task;
	unsigned int vpn = badVaddr >> 12;
	unsigned int acid = current_task->ASID;
	vpn &= 0x1ffff;  //19 bits
	//unsigned int test = current_task->ASID;
	pgd_t *pgd = current_task->mm->pgd;
	unsigned int pgdc = (unsigned int)pgd;
	unsigned int ptec = pgdc + vpn * sizeof( PageTableEntry );
	pte_t *pte = (pte_t *)ptec;
	pte->EntryLo0.PFN = pfn;
	pte->EntryLo0.V = 1;
	pte->EntryHi.VPN2 = badVaddr;
	pte->EntryHi.ASID = acid;
#ifdef TLB_DEBUG
	kernel_printf( "sucessfully write_page_table(pte, pfn).\n" );
#endif  //TLB_DEBUG
}