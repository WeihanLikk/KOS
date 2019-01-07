#ifndef _KERNEL_H
#define _KERNEL_H

#undef NULL
#if defined( __cplusplus )
#define NULL 0
#else
#define NULL ( (void *)0 )
#endif

#define min( x, y ) ( { \
	typeof(x) _x = (x);	\
	typeof(y) _y = (y);	\
	(void) (&_x == &_y);		\
	_x < _y ? _x : _y; } )

#define max( x, y ) ( { \
	typeof(x) _x = (x);	\
	typeof(y) _y = (y);	\
	(void) (&_x == &_y);		\
	_x > _y ? _x : _y; } )

#define offsetof( TYPE, MEMBER ) ( (unsigned int)&( (TYPE *)0 )->MEMBER )

#define container_of( ptr, type, member ) ( { \
    const typeof( ((type *)0)->member ) *__mptr = (ptr); \
    (type *)( (char *)__mptr - offsetof(type,member) ); } )

#define likely( x ) __builtin_expect( !!( x ), 1 )
#define unlikely( x ) __builtin_expect( !!( x ), 0 )
#define fastcall __attribute__( ( regparm( 3 ) ) )

#define do_div( a, b ) xfs_do_div( &( a ), ( b ), sizeof( a ) )

#define swap( a, b )             \
	do                           \
	{                            \
		typeof( a ) tmp = ( a ); \
		( a ) = ( b );           \
		( b ) = tmp;             \
	} while ( 0 )

#define for_each_sched_entity( se ) \
	for ( ; se; se = NULL )

static inline unsigned int xfs_do_div( void *a, unsigned int b, int n )
{
	unsigned int mod;

	switch ( n )
	{
	case 4:
		mod = *(unsigned int *)a % b;
		*(unsigned int *)a = *(unsigned int *)a / b;
		return mod;
	case 8:
		mod = do_div( *(unsigned int *)a, b );
		return mod;
	}

	/* NOTREACHED */
	return 0;
}

#define schedstat_add( rq, field, amt ) \
	do                                  \
	{                                   \
		( rq )->field += ( amt );       \
	} while ( 0 )

#endif