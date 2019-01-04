#ifndef _ZJUNIX_PAGH_H
#define _ZJUNIX_PAGE_H

#include <page.h>
#include <kos/list.h>

#ifndef PAGE_SHIFT
#define PAGE_SHIFT 12  //size of page
#endif

#ifndef PAGE_SIZE
#define PAGE_SIZE ( 1 << PAGE_SHIFT )
#endif

#define PAGE_MASK ( ~( PAGE_SIZE - 1 ) )

#define INDEX_MASK 0x3ff  //低10位
#define PGD_SHIFT 22
#define PGD_SIZE ( 1 << PAGE_SHIFT )
#define PGD_MASK ( ~( ( 1 << PGD_SHIFT ) - 1 ) )

#endif