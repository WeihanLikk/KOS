#include "ps.h"
#include <driver/ps2.h>
#include <driver/sd.h>
#include <driver/vga.h>
#include <zjunix/bootmm.h>
#include <zjunix/buddy.h>
#include <zjunix/fs/fat.h>
#include <zjunix/vfs/vfs.h>
#include <zjunix/slab.h>
#include <zjunix/time.h>
#include <zjunix/utils.h>
#include <driver/vga.h>
#include "../usr/ls.h"
#include "exec.h"
#include "myvi.h"

char ps_buffer[ 64 ];
int ps_buffer_index;

struct lock_t lk;

extern struct dentry *pwd_dentry;

void test_proc()
{
	unsigned int timestamp;
	unsigned int currTime;
	unsigned int data;
	asm volatile( "mfc0 %0, $9, 6\n\t"
				  : "=r"( timestamp ) );
	data = timestamp & 0xff;
	while ( 1 )
	{
		asm volatile( "mfc0 %0, $9, 6\n\t"
					  : "=r"( currTime ) );
		if ( currTime - timestamp > 100000000 )
		{
			timestamp += 100000000;
			*( (unsigned int *)0xbfc09018 ) = data;
		}
	}
}

void testMem( unsigned int size )
{
	int i;
	int total = 5;
	//int sizeArr[200];
	int *addrArr[ 200 ];
	//for (i=0; i<total; i++) sizeArr[i] = 100;
	buddy_info();
	// for (i=10; i<100; i++) sizeArr[i] = 2<<12;
	// for (i=100; i<total; i++) sizeArr[i] = 4<<12;
	kernel_printf( "Allocate %d times *  %d pages:\n", total, size );
	for ( i = 0; i < total; i++ )
	{
		addrArr[ i ] = alloc_pages( size );
		kernel_printf( "#%d: %x\t", i, addrArr[ i ] );
		if ( i % 3 == 2 ) kernel_printf( "\n" );
	}
	kernel_printf( "Allocate success!\n" );
	// bootmap_info("bootmm");
	buddy_info();
	// kernel_getkey();
	kernel_printf( "Deallocate the pages:\n" );
	int bplevel = 0;
	while ( 1 << bplevel < size )
	{
		bplevel++;
	}
	for ( i = 0; i < total; i++ )
	{
		free_pages( addrArr[ i ], bplevel );
		//kernel_printf("addr%d %x done\t", i, addrArr[i]);
		// if(i==4) kernel_printf("\n");
	}
	kernel_printf( "Deallocate all pages success!\n" );
	// bootmap_info("bootmm");
	buddy_info();
}

void testMem2( unsigned int size )
{
	int i;
	int total = 10;
	int sizeArr[ 200 ];
	int *addrArr[ 200 ];
	for ( i = 0; i < total; i++ ) sizeArr[ i ] = size;
	buddy_info();
	// for (i=10; i<100; i++) sizeArr[i] = 2<<12;
	// for (i=100; i<total; i++) sizeArr[i] = 4<<12;
	kernel_printf( "Allocate %d blocks of %d byte:\n", 10, size );
	for ( i = 0; i < total; i++ )
	{
		addrArr[ i ] = kmalloc( sizeArr[ i ] );
		kernel_printf( "#%d: %x\t", i, addrArr[ i ] );
		if ( i % 3 == 2 ) kernel_printf( "\n" );
	}
	kernel_printf( "Allocate success!\n" );
	// bootmap_info("bootmm");
	buddy_info();
	// kernel_getkey();

	for ( i = 0; i < total; i++ )
	{
		kfree( addrArr[ i ] );
		// kernel_printf("addr%d %x done\n", i, addrArr[i]);
	}
	kernel_printf( "Deallocate all blocks success!\n" );
	// bootmap_info("bootmm");
	buddy_info();
}
void testMem3( unsigned int size )
{
	kernel_printf( "******now test dynamically creating and deleting cache*****\n" );
	int i;
	int total = 10;
	int sizeArr[ 200 ];
	int *addrArr[ 200 ], *addrArr2[ 200 ];
	for ( i = 0; i < total; i++ ) sizeArr[ i ] = size;
	// for (i=10; i<100; i++) sizeArr[i] = 2<<12;
	// for (i=100; i<total; i++) sizeArr[i] = 4<<12;
	kernel_printf( "Dynamic Allocating...\n", 10, size );
	kernel_printf( "Allocate %d blocks of objects sized %d byte \n", 6, size );
	for ( i = 0; i < total; i++ )
	{
		addrArr[ i ] = kmalloc_object_pool( sizeArr[ i ], 0 );
		kernel_printf( "#%d: %x\t", i, addrArr[ i ] );
		if ( i % 3 == 2 ) kernel_printf( "\n" );
	}
	kernel_printf( "Allocate success!\n" );
	int cache_number = kmem_cache_number();
	slab_info();
	kernel_printf( "******dynamically creating cache test success*****\n" );
	// bootmap_info("bootmm");
	// kernel_getkey();
	kernel_getchar();
	kernel_printf( "Allocate %d blocks of a different kind objects sized %d byte \n", 6, size );
	for ( i = 0; i < total; i++ )
	{
		addrArr2[ i ] = kmalloc_object_pool( sizeArr[ i ], 1 );
		kernel_printf( "#%d: %x\t", i, addrArr2[ i ] );
		if ( i % 3 == 2 ) kernel_printf( "\n" );
	}
	kernel_printf( "Allocate success!\n" );
	slab_info();
	kernel_printf( "******object pool test success*****\n" );
	kernel_getchar();
	for ( i = 0; i < total; i++ )
	{
		kfree_object_pool( addrArr[ i ] );
		// kernel_printf("addr%d %x done\n", i, addrArr[i]);
	}
	kernel_printf( "Deallocate all blocks success!\n" );
	obj_cache_delete( 3, 0 );
	kernel_printf( "Delete the first cache of size %d success!\n", size );
	slab_info();
	kernel_getchar();
	//bootmap_info("bootmm");
	for ( i = 0; i < total; i++ )
	{
		kfree_object_pool( addrArr2[ i ] );
		// kernel_printf("addr%d %x done\n", i, addrArr[i]);
	}
	kernel_printf( "Deallocate all blocks success!\n" );
	obj_cache_delete( 3, 1 );
	kernel_printf( "Delete the second cache of size %d success!\n", size );
	slab_info();
	kernel_printf( "******dynamically deleting cache test success*****\n" );
}

void test_sync()
{
	int i, j;
	lockup( &lk );
	for ( i = 0; i < 10; i++ )
	{
		kernel_printf( "%d%d%d\n", i, i, i );
		//kernel_getchar();
		for ( j = 0; j < 100000; j++ )
		{
		}
	}
	unlock( &lk );
	while ( 1 )
	{
	}
}

int sync_demo_create()
{
	// init_lock(&lk);
	// int asid = pc_peek();
	// if (asid < 0) {
	//     kernel_puts("Failed to allocate pid.\n", 0xfff, 0);
	//     return 1;
	// }
	// unsigned int init_gp;
	// asm volatile("la %0, _gp\n\t" : "=r"(init_gp));
	// pc_create(asid, test_sync, (unsigned int)kmalloc(4096), init_gp, "sync1");

	// asid = pc_peek();
	// if (asid < 0) {
	//     kernel_puts("Failed to allocate pid.\n", 0xfff, 0);
	//     return 1;
	// }
	// asm volatile("la %0, _gp\n\t" : "=r"(init_gp));
	// pc_create(asid, test_sync, (unsigned int)kmalloc(4096), init_gp, "sync2");
	// return 0;
}

// int proc_demo_create() {
//     int asid = pc_peek();
//     if (asid < 0) {
//         kernel_puts("Failed to allocate pid.\n", 0xfff, 0);
//         return 1;
//     }
//     unsigned int init_gp;
//     asm volatile("la %0, _gp\n\t" : "=r"(init_gp));
//     pc_create(asid, test_proc, (unsigned int)kmalloc(4096), init_gp, "test");
//     return 0;
// }

void ps()
{
	kernel_printf( "Press any key to enter shell.\n" );
	kernel_getchar();
	char c;
	ps_buffer_index = 0;
	ps_buffer[ 0 ] = 0;
	kernel_clear_screen( 31 );
	// kernel_puts("PS>", VGA_WHITE, VGA_BLACK);
	// kernel_puts("PowerShell\n", VGA_WHITE, VGA_BLACK);
	kernel_puts( "PS", VGA_GREEN, VGA_BLACK );
	kernel_puts( ":", VGA_WHITE, VGA_BLACK );
	kernel_puts( pwd_dentry->d_name.name, VGA_YELLOW, VGA_BLACK );
	kernel_puts( ">", VGA_WHITE, VGA_BLACK );
	while ( 1 )
	{
		c = kernel_getchar();
		if ( c == '\n' )
		{
			ps_buffer[ ps_buffer_index ] = 0;
			if ( kernel_strcmp( ps_buffer, "exit" ) == 0 )
			{
				ps_buffer_index = 0;
				ps_buffer[ 0 ] = 0;
				kernel_printf( "\nPowerShell exit.\n" );
			}
			else
				parse_cmd();
			ps_buffer_index = 0;
			// kernel_puts("PS>", VGA_WHITE, VGA_BLACK);
			// kernel_puts("PowerShell\n", VGA_WHITE, VGA_BLACK);
			kernel_puts( "PS", VGA_GREEN, VGA_BLACK );
			kernel_puts( ":", VGA_WHITE, VGA_BLACK );
			kernel_puts( pwd_dentry->d_name.name, VGA_YELLOW, VGA_BLACK );
			kernel_puts( ">", VGA_WHITE, VGA_BLACK );
		}
		else if ( c == 0x08 )
		{
			if ( ps_buffer_index )
			{
				ps_buffer_index--;
				kernel_putchar_at( ' ', 0xfff, 0, cursor_row, cursor_col - 1 );
				cursor_col--;
				kernel_set_cursor();
			}
		}
		else
		{
			if ( ps_buffer_index < 63 )
			{
				ps_buffer[ ps_buffer_index++ ] = c;
				kernel_putchar( c, 0xfff, 0 );
			}
		}
	}
}

void parse_cmd()
{
	unsigned int result = 0;
	char dir[ 32 ];
	char c;
	kernel_putchar( '\n', 0, 0 );
	char sd_buffer[ 8192 ];
	int i = 0;
	char *param;
	for ( i = 0; i < 63; i++ )
	{
		if ( ps_buffer[ i ] == ' ' )
		{
			ps_buffer[ i ] = 0;
			break;
		}
	}
	if ( i == 63 )
	{
		ps_buffer[ 62 ] = 0;
		param = ps_buffer + 62;
	}
	else
		param = ps_buffer + i + 1;
	if ( ps_buffer[ 0 ] == 0 )
	{
		return;
	}
	else if ( kernel_strcmp( ps_buffer, "clear" ) == 0 )
	{
		kernel_clear_screen( 31 );
	}
	else if ( kernel_strcmp( ps_buffer, "echo" ) == 0 )
	{
		kernel_printf( "%s\n", param );
	}
	else if ( kernel_strcmp( ps_buffer, "gettime" ) == 0 )
	{
		char buf[ 10 ];
		get_time( buf, sizeof( buf ) );
		kernel_printf( "%s\n", buf );
	}
	else if ( kernel_strcmp( ps_buffer, "sdwi" ) == 0 )
	{
		for ( i = 0; i < 512; i++ )
			sd_buffer[ i ] = i;
		sd_write_block( sd_buffer, 23128, 1 );
		kernel_puts( "sdwi\n", 0xfff, 0 );
	}
	else if ( kernel_strcmp( ps_buffer, "sdr" ) == 0 )
	{
		for ( i = 0; i < 512; i++ )
			sd_buffer[ i ] = 0;

		i = sd_read_block( sd_buffer, 23128, 1 );

		kernel_printf( "read_result: %d\n", i );
		for ( i = 0; i < 512; i++ )
		{
			kernel_printf( "%d ", sd_buffer[ i ] );
		}
		kernel_putchar( '\n', 0xfff, 0 );
	}
	else if ( kernel_strcmp( ps_buffer, "sdwz" ) == 0 )
	{
		for ( i = 0; i < 512; i++ )
		{
			sd_buffer[ i ] = 0;
		}
		sd_write_block( sd_buffer, 23128, 1 );
		kernel_puts( "sdwz\n", 0xfff, 0 );
	}

	else if ( kernel_strcmp( ps_buffer, "cat" ) == 0 )
	{
		result = vfs_cat( param );
	}
	else if ( kernel_strcmp( ps_buffer, "rm" ) == 0 )
	{
		result = vfs_rm( param );
	}
	else if ( kernel_strcmp( ps_buffer, "ls" ) == 0 )
	{
		result = vfs_ls( param );
	}
	else if ( kernel_strcmp( ps_buffer, "cd" ) == 0 )
	{
		result = vfs_cd( param );
	}
	else if ( kernel_strcmp( ps_buffer, "mnt" ) == 0 )
	{
		// kernel_printf( "In mount\nparam = %s", param );

		result = vfs_mnt( param );
	}
	else if ( kernel_strcmp( ps_buffer, "umnt" ) == 0 )
	{
		result = vfs_umnt( param );
	}
	else if ( kernel_strcmp( ps_buffer, "mmdir" ) == 0 )
	{
		result = vfs_create( param );
	}
	else if ( kernel_strcmp( ps_buffer, "exec" ) == 0 )
	{
		result = execk( 1, (void *)param, 0, 0 );
	}
	else if ( kernel_strcmp( ps_buffer, "ewait" ) == 0 )
	{
		result = execkwait( 1, (void *)param );
	}
	else if ( kernel_strcmp( ps_buffer, "kill" ) == 0 )
	{
		int pid = param[ 0 ] - '0';
		result = pc_kill( pid );
	}
	else if ( kernel_strcmp( ps_buffer, "wait" ) == 0 )
	{
		result = execk( 1, (void *)param, 1, 0 );
	}
	else if ( kernel_strcmp( ps_buffer, "waitpid" ) == 0 )
	{
		int pid = param[ 0 ] - '0';
		waitpid( pid );
	}
	else if ( kernel_strcmp( ps_buffer, "ps" ) == 0 )
	{
		print_info();
	}
	else if ( kernel_strcmp( ps_buffer, "nice" ) == 0 )
	{
		if ( param[ 0 ] != ' ' )
		{
			int increase;
			if ( param[ 0 ] == '-' )
			{
				int a = param[ 1 ] - '0';
				int b = param[ 2 ] - '0';
				increase = ( 10 * a + b ) * -1;
			}
			else
			{
				int a = param[ 0 ] - '0';
				int b = param[ 1 ] - '0';
				increase = 10 * a + b;
			}

			sys_prioiry( increase );
		}
		else
		{
			kernel_printf( "Error in format, the first argument is a blank\n" );
		}
		//sys_prioiry( increase );
	}
	else if ( kernel_strcmp( ps_buffer, "priomax" ) == 0 )
	{
		if ( param[ 0 ] != ' ' )
		{
			int pid = param[ 0 ] - '0';
			sys_prioiry_pid( -40, pid );
		}
		else
		{
			kernel_printf( "Error in format, the first argument is a blank\n" );
		}
	}
	else if ( kernel_strcmp( ps_buffer, "testvm" ) == 0 )
	{
		result = execk( 1, (void *)param, 0, 1 );
		//kernel_printf(ps_buffer, "execk3 return with %d\n",result);
	}
	else if ( kernel_strcmp( ps_buffer, "testvma" ) == 0 )
	{
		result = execvm( 1, (void *)param, 0, 1 );
		//kernel_printf(ps_buffer, "execk4 return with %d\n",result);
	}
	else if ( kernel_strcmp( ps_buffer, "testb" ) == 0 )
	{
		int size = 0;
		for ( int i = 0; param[ i ] != '\0'; i++ )
		{
			size = size * 10 + param[ i ] - '0';
		}
		kernel_printf( "Testing Buddy System...\n" );
		testMem( size );
	}
	else if ( kernel_strcmp( ps_buffer, "tests" ) == 0 )
	{
		int size = 0;
		for ( int i = 0; param[ i ] != '\0'; i++ )
		{
			size = size * 10 + param[ i ] - '0';
		}
		kernel_printf( "Testing Slab System...\n" );
		testMem2( size );
	}
	else if ( kernel_strcmp( ps_buffer, "testso" ) == 0 )
	{
		int size = 0;
		for ( int i = 0; param[ i ] != '\0'; i++ )
		{
			size = size * 10 + param[ i ] - '0';
		}
		kernel_printf( "Testing Slab System's object pool...\n" );
		testMem3( size );
	}

	else
	{
		kernel_puts( ps_buffer, 0xfff, 0 );
		kernel_puts( ": command not found\n", 0xfff, 0 );
	}
}
