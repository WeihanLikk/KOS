#include <arch.h>
#include <driver/ps2.h>
#include <driver/vga.h>
#include <exc.h>
#include <intr.h>
#include <page.h>
#include <kos/mm/bootmm.h>
#include <kos/mm/buddy.h>
//#include <kos/fs/fat.h>
#include <kos/log.h>
#include <kos/pc/sched.h>
#include <kos/mm/slab.h>
#include <kos/tlbload.h>
#include <kos/syscall.h>
#include <kos/time.h>

void machine_info()
{
	int row;
	int col;
	kernel_printf( "\n%s\n", "kos v1.0" );
	row = cursor_row;
	col = cursor_col;
	cursor_row = 29;
	kernel_printf( "%s", "Created by Group 13, Zhejiang University." );
	cursor_row = row;
	cursor_col = col;
	kernel_set_cursor();
}

#pragma GCC push_options
#pragma GCC optimize( "O0" )
// void create_startup_process()
// {
// 	int res;

// 	res = task_fork( "kernel_shell", (void *)ps, 0, 0, 0, 0 );
// 	if ( res == 0 )
// 		kernel_printf( "create startup process failed\n" );
// 	else
// 		kernel_printf( "kernel shell created\n" );
// }
#pragma GCC pop_options

void init_kernel()
{
	kernel_clear_screen( 31 );
	// Exception
	init_exception();
	// Page table
	init_pgtable();
	// Drivers
	init_vga();
	init_ps2();
	// Memory management
	log( LOG_START, "Memory Modules." );
	init_bootmm();
	log( LOG_OK, "Bootmem." );
	init_buddy();
	log( LOG_OK, "Buddy." );
	init_slab();
	log( LOG_OK, "Slab." );
	log( LOG_END, "Memory Modules." );
	// virtual memory
	init_tlbload();
	log( LOG_OK, "TLB LOAD." );
	// File system
	//log( LOG_START, "File System." );
	//init_fs();
	//log( LOG_END, "File System." );
	// System call
	log( LOG_START, "System Calls." );
	init_syscall();
	log( LOG_END, "System Calls." );
	// Process control
	log( LOG_START, "Process Control Module." );
	sched_init();
	//create_startup_process();
	log( LOG_END, "Process Control Module." );
	// Interrupts
	log( LOG_START, "Enable Interrupts." );
	init_interrupts();

	log( LOG_END, "Enable Interrupts." );

	// Init finished
	machine_info();
	*GPIO_SEG = 0x11223344;
	// Enter shell
	while ( 1 )
		;
}
