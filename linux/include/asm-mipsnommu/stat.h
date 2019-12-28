#ifndef _ASM_STAT_H
#define _ASM_STAT_H

//#include <linux/types.h>

struct __old_kernel_stat {
	unsigned int	st_dev;
	unsigned int	st_ino;
	unsigned int	st_mode;
	unsigned int	st_nlink;
	unsigned int	st_uid;
	unsigned int	st_gid;
	unsigned int	st_rdev;
	long		st_size;
	unsigned int	st_atime, st_res1;
	unsigned int	st_mtime, st_res2;
	unsigned int	st_ctime, st_res3;
	unsigned int	st_blksize;
	int		st_blocks;
	unsigned int	st_unused0[2];
};

struct stat {
	unsigned int		st_dev;
	long		st_pad1[3];		/* Reserved for network id */
	unsigned long		st_ino;
	unsigned int		st_mode;
	int		st_nlink;
	int		st_uid;
	int		st_gid;
	unsigned int		st_rdev;
	long		st_pad2[2];
	long		st_size;
	long		st_pad3;
	/*
	 * Actually this should be timestruc_t st_atime, st_mtime and st_ctime
	 * but we don't have it under Linux.
	 */
	long		st_atime;
	long		reserved0;
	long		st_mtime;
	long		reserved1;
	long		st_ctime;
	long		reserved2;
	long		st_blksize;
	long		st_blocks;
	long		st_pad4[14];
};

/*
 * This matches struct stat64 in glibc2.1, hence the absolutely insane
 * amounts of padding around unsigned int's.  The memory layout is the same as of
 * struct stat of the 64-bit kernel.
 */

struct stat64 {
	unsigned long	st_dev;
	unsigned long	st_pad0[3];	/* Reserved for st_dev expansion  */

	unsigned long long	st_ino;

	unsigned int		st_mode;
	int		st_nlink;

	int		st_uid;
	int		st_gid;

	unsigned long	st_rdev;
	unsigned long	st_pad1[3];	/* Reserved for st_rdev expansion  */

	long long	st_size;

	/*
	 * Actually this should be timestruc_t st_atime, st_mtime and st_ctime
	 * but we don't have it under Linux.
	 */
	long		st_atime;
	unsigned long	reserved0;	/* Reserved for st_atime expansion  */

	long		st_mtime;
	unsigned long	reserved1;	/* Reserved for st_mtime expansion  */

	long		st_ctime;
	unsigned long	reserved2;	/* Reserved for st_ctime expansion  */

	unsigned long	st_blksize;
	unsigned long	st_pad2;

	long long	st_blocks;
};

#endif /* _ASM_STAT_H */
