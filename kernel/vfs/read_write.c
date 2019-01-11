#include <zjunix/vfs/vfs.h>
#include <zjunix/vfs/vfscache.h>

#include <zjunix/slab.h>
#include <zjunix/utils.h>
#include <driver/vga.h>
#include <zjunix/log.h>

extern struct cache *pcache;

/* TODO : modify ext2/fat32 initialization
          modify macro READ_FILE/WRITE_FILE
          function declaration page_to_cache
          */

u32 vfs_read( struct file *fp, char *buf, u32 count, u32 *ppos )
{
	if ( !( fp->f_mode & FMODE_READ ) )
		return -EBADF;
	if ( !fp->f_op || !fp->f_op->read )
		return -EINVAL;

	return fp->f_op->read( fp, buf, count, ppos, READ_FILE );
}

u32 vfs_write( struct file *fp, char *buf, u32 count, u32 *ppos )
{
	if ( !( fp->f_mode & FMODE_READ ) )
		return -EBADF;
	if ( !fp->f_op || !fp->f_op->write )
		return -EINVAL;

	return fp->f_op->write( fp, buf, count, ppos, WRITE_FILE );
}

u32 generic_file_read_write( struct file *fp, u8 *buf, u32 count, u32 *ppos, u32 isread )
{
	struct inode *inode = fp->f_dentry->d_inode;
	struct address_space *mapping = &( inode->i_data );

	u32 pos = *ppos;
	// TODO: page -- blksize
	u32 blksize = inode->i_blksize;
	u32 startPageNo = pos / blksize;
	u32 startPageCur = pos % blksize;
	u32 endPageNo, endPageCur;

	if ( isread && pos + count < inode->i_size )
	{
		endPageNo = ( pos + count ) / blksize;
		endPageCur = ( pos + count ) % blksize;
	}
	else
	{
		endPageNo = inode->i_size / blksize;
		endPageCur = inode->i_size % blksize;
	}

	// kernel_printf( "StartPageNo = %d, endPageNo = %d, startPageCur = %d, endPageNo = %d\n", startPageNo, endPageNo, startPageCur, endPageCur );

	u32 cur = 0;
	for ( u32 pageNo = startPageNo; pageNo <= endPageNo; pageNo++ )
	{
		u32 r_page = mapping->a_op->bmap( inode, pageNo );  // file relative page address -> physical address
		struct condition cond = {
			.cond1 = (void *)( &r_page ),
			.cond2 = (void *)( fp->f_dentry->d_inode ),
		};
		// TODO: pcache_look_up mechanism
		struct vfs_page *curPage = (struct vfs_page *)pcache->c_op->look_up( pcache, &cond );
		if ( !curPage )
		{
			curPage = page_to_cache( r_page, mapping );
			if ( !curPage )
				goto out;
		}

		// data copy
		u32 curStartPageCur = ( pageNo == startPageNo ) ? startPageCur : 0;
		u32 curEndPageCur = ( pageNo == endPageNo ) ? endPageCur : blksize;
		u32 Count = curEndPageCur - curStartPageCur;
		if ( isread )
			kernel_memcpy( buf + cur, curPage->p_data + curStartPageCur, Count );
		else
		{
			kernel_memcpy( curPage->p_data + curStartPageCur, buf + cur, Count );
			// adopt write through mechanism
			mapping->a_op->writepage( curPage );
		}
		cur += Count;
		*ppos += Count;
		// kernel_printf( "buf = %s\n", buf );
		// kernel_printf( "Count = %d\n", Count );
	}
	if ( !isread && inode->i_size )
	{
		inode->i_size = pos + count;
		struct dentry *parent = fp->f_dentry->d_parent;
		// adopt write through mechanism
		inode->i_sb->s_op->write_inode( inode, parent );
	}

out:
	fp->f_pos = *ppos;
	return cur;
}

struct vfs_page *page_to_cache( u32 r_page, struct address_space *mapping )
{
	struct vfs_page *newPage = (struct vfs_page *)kmalloc( sizeof( struct vfs_page ) );
	if ( !newPage )
		goto out;

	newPage->p_state = P_CLEAR;
	newPage->p_location = r_page;
	newPage->p_mapping = mapping;
	INIT_LIST_HEAD( &newPage->p_hash );
	INIT_LIST_HEAD( &newPage->p_LRU );
	INIT_LIST_HEAD( &newPage->p_list );

	// NEED CHECK
	if ( newPage->p_mapping->a_op->readpage( newPage ) )
	{
		release_page( newPage );
		goto out;
	}
	pcache->c_op->add( pcache, (void *)newPage );
	// NEED CHECK
	list_add( &newPage->p_list, &mapping->a_cache );

out:
	return newPage;
}

u32 generic_file_flush( struct file *fp )
{
	struct address_space *mapping = &( fp->f_dentry->d_inode->i_data );
	struct list_head *begin = &( mapping->a_cache );
	struct list_head *a = begin;
	struct vfs_page *page;
	while ( a != begin )
	{
		page = container_of( a, struct vfs_page, p_list );
		if ( page->p_state & P_DIRTY )
			mapping->a_op->writepage( page );
		a = a->next;
	}
	return 0;
}