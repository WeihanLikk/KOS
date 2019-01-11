#include <zjunix/vfs/vfs.h>
#include <zjunix/vfs/vfscache.h>
// not elegant
#include <zjunix/vfs/ext2.h>

#include <driver/vga.h>
#include <zjunix/utils.h>
#include <zjunix/slab.h>

// 外部变量
extern struct cache *dcache;
extern struct cache *icache;
extern struct cache *pcache;
extern struct dentry *pwd_dentry;
extern struct vfsmount *pwd_mnt;

// 以下为Power shell 的接口
// 输出文件的内容
u32 vfs_cat( const u8 *path )
{
	u8 *buf;
	u32 err;
	u32 base;
	u32 file_size;
	struct file *file;

	// 调用VFS提供的打开接口
	file = vfs_open( path, O_RDONLY, base );
	if ( IS_ERR_OR_NULL( file ) )
	{
		if ( PTR_ERR( file ) == -ENOENT )
			kernel_printf( "File not found!\n" );
		return PTR_ERR( file );
	}

	// 接下来读取文件数据区的内容到buf
	base = 0;
	file_size = file->f_dentry->d_inode->i_size;

	buf = (u8 *)kmalloc( file_size + 1 );
	if ( vfs_read( file, buf, file_size, &base ) != file_size )
		return 1;

	// 打印buf里面的内容
	buf[ file_size ] = 0;
	kernel_printf( "%s\n", buf );

	// 关闭文件并释放内存
	err = vfs_close( file );
	if ( err )
		return err;

	kfree( buf );
	return 0;
}

u32 vfs_vi( const u8 *tot )
{
	// u8 *path = tot;
	// u8 *content = tot;
	// while ( *content++ != ' ' )
	// 	;
	// *( content - 1 ) = '\0';

	// struct file *file = vfs_open( path, O_RDONLY, base );
	// if ( IS_ERR_OR_NULL( file ) )
	// {
	// 	if ( PTR_ERR( file ) == -ENOENT )
	// 		kernel_printf( "File not found!\n" );
	// 	return PTR_ERR( file );
	// }

	return 0;
}

// 更改当前的工作目录
u32 vfs_cd( const u8 *path )
{
	u32 err;
	struct nameidata nd;

	// 调用VFS提供的查找接口
	err = path_lookup( path, LOOKUP_DIRECTORY, &nd );
	if ( err == -ENOENT )
	{
		kernel_printf( "No such directory!\n" );
		return err;
	}
	else if ( IS_ERR_VALUE( err ) )
	{
		return err;
	}

	// 若成功则改变相应的dentry和mnt
	pwd_dentry = nd.dentry;
	pwd_mnt = nd.mnt;

	return 0;
}

// 输出文件夹的内容
u32 vfs_ls( const u8 *path )
{
	u32 i;
	u32 err;
	struct file *file;
	struct getdent getdent;

	// 调用VFS提供的打开接口
	if ( path[ 0 ] == 0 )
		file = vfs_open( ".", LOOKUP_DIRECTORY, 0 );
	else
		file = vfs_open( path, LOOKUP_DIRECTORY, 0 );

	if ( IS_ERR_OR_NULL( file ) )
	{
		if ( PTR_ERR( file ) == -ENOENT )
			kernel_printf( "Directory not found!\n" );
		else
			kernel_printf( "Other error: %d\n", -PTR_ERR( file ) );
		return PTR_ERR( file );
	}

	// 调用具体文件系统的readir函数
	err = file->f_op->readdir( file, &getdent );
	if ( err )
		return err;

	// 接下来往屏幕打印结果
	for ( i = 0; i < getdent.count; i++ )
	{
		if ( getdent.dirent[ i ].type == FT_DIR )
			kernel_puts( getdent.dirent[ i ].name, VGA_YELLOW, VGA_BLACK );
		else if ( getdent.dirent[ i ].type == FT_REG_FILE )
			kernel_puts( getdent.dirent[ i ].name, VGA_WHITE, VGA_BLACK );
		else
			kernel_puts( getdent.dirent[ i ].name, VGA_GREEN, VGA_BLACK );
		kernel_printf( " " );
	}
	kernel_printf( "\n" );

	return 0;
}

// 删除一个文件（不能是文件夹）
u32 vfs_rm( const u8 *path )
{
	u32 err;
	struct inode *inode;
	struct dentry *dentry;
	struct nameidata nd;

	// 调用VFS提供的查找接口
	err = path_lookup( path, 0, &nd );
	if ( IS_ERR_VALUE( err ) )
	{
		if ( err == -ENOENT )
			kernel_printf( "File not found!\n" );
		return err;
	}

	// 先删除inode对应文件在外存上的相关信息
	dentry = nd.dentry;
	err = dentry->d_inode->i_sb->s_op->delete_inode( dentry );
	if ( err )
		return err;

	// 最后只需要在缓存中删去inode即可，page和dentry都允许保留
	dentry->d_inode = 0;

	return 0;
}

u32 vfs_mnt( const u8 *tot )
{
	u8 *fsname = tot;
	u8 *path = tot;
	while ( *path++ != ' ' )
		;
	*( path - 1 ) = '\0';
	// kernel_printf( "fsname = %s, path = %s", fsname, path );

	struct nameidata nd;
	u32 err = path_lookup( path, 0, &nd );
	if ( IS_ERR_VALUE( err ) )
	{
		if ( err == -ENOENT )
			kernel_printf( "File not found!\n" );
		return err;
	}

	// kernel_printf( "fsname = %s, path = %s", fsname, path );
	mount_fs( fsname, nd.dentry );

	return 0;
}

// path_lookup 会直接完成follow_mount 考虑处理方案
u32 vfs_umnt( const u8 *path )
{
	// u8 *fsname = tot;
	// u8 *path = tot;
	// while ( *path++ != ' ' )
	// 	;
	// *( path - 1 ) = '\0';

	struct nameidata nd;
	nd.last_type = LAST_NORM;
	u32 err = path_lookup( path, 0, &nd );
	// struct list_head *begin = &( nd.dentry->d_subdirs );
	// struct list_head *a = begin->next;
	// struct dentry *dentry;
	// while ( a != begin )
	// {
	// 	dentry = list_entry( a, struct dentry, d_subdirs );
	// 	kernel_printf( "dentry_name: %s\nfsname: %s\nname_len: %d", dentry->d_name.name, fsname, dentry->d_name.len );
	// 	kernel_printf( "inode is null :%d", dentry->d_inode == 0 );
	// 	// if ( dentry->d_mounted && kernel_strcmp( dentry->d_name.name, fsname ) == 0 )
	// 	if ( dentry->d_mounted ) break;
	// 	a = a->next;
	// }

	// if ( a == begin )
	// {
	// 	kernel_printf( "No such file system subdir!\n" );
	// 	goto out;
	// }

	if ( IS_ERR_VALUE( err ) )
	{
		if ( err == -ENOENT )
			kernel_printf( "File not found!\n" );
		return err;
	}

	if ( nd.dentry->d_pinned )
	{
		kernel_printf( "cannot unmount pinned entry!\n" );
		goto out;
	}
	kernel_printf( "Here starts unmount\n" );
	unmount_fs( nd.dentry );

out:
	return 0;
}

u32 vfs_create( u8 *tot )
{
	u8 *nodename = tot;
	u8 *path = tot;
	while ( *path++ != ' ' )
		;
	*( path - 1 ) = '\0';

	struct nameidata nd;
	u32 err = path_lookup( path, 0, &nd );
	if ( IS_ERR_VALUE( err ) )
	{
		if ( err == -ENOENT )
			kernel_printf( "File not found!\n" );
		return err;
	}

	struct qstr name;
	name.name = (u8 *)kmalloc( ( path - tot ) * sizeof( u8 ) );
	kernel_strcpy( name.name, nodename );
	name.len = path - tot - 1;

	ext2_create_inode( nd.dentry, &name );
}