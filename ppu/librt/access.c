#include <stdio.h>
#include <fcntl.h>
#include <_ansi.h>
#include <_syslist.h>
#include <sys/reent.h>
#include <sys/errno.h>
#include <sys/types.h>
/*
#include <sys/lv2errno.h>

#include <sys/file.h>
*/
#include <sys/stat.h>
#include <unistd.h>


int _access(const char *fn, int flags, int* _errno)
{
	struct stat s;

	/* Did the pass us the right flags? */
	if ( (flags != F_OK) && (flags != R_OK) && (flags != W_OK) && (flags != X_OK) ) {
		/* Nope. */
		*_errno = EINVAL;
		return -1;
	}

	if (stat(fn, &s)) {
		/* return -1 because the file does not exist or pathname is too long. */
		return -1;
	}

	if (flags == F_OK) {
		/* We know the file exists because stat didn't fail. */
		return 0;
	}

	if (flags & W_OK) {
		/* Do we have write permission to the file? */
		if (s.st_mode & S_IWRITE) {
			/* We do. */
			return 0;
		}

		/* Nope. */
		*_errno = EACCES;
		return -1;
	}

	if (flags & R_OK) {
		/* Do we have read permission to the file? */
		if (s.st_mode & S_IREAD) {
			/* We do. */
			return 0;
		}

		/* Nope. */
		*_errno = EACCES;
		return -1;
	}

	if (flags & X_OK) {
		/* Do we have executable permission to the file? */
		if (s.st_mode & S_IEXEC) {
			/* We do. */
			return 0;
		}

		/* Nope. */
		*_errno = EACCES;
		return -1;
	}

	/* We should never reach this, ever...in case we do though, lets return -1. */
	/* and set errno to ENOSYS (Function not implemented */
	*_errno = ENOSYS;
	return -1;
}

int
_DEFUN(__librt_access_r,(r,path,amode),
	   struct _reent *r _AND
	   const char *path _AND
	   int amode)
{
/*
	Lv2 syscall 816 (sys_fs_access) crashes, so we use a standard implementation 
	return lv2errno_r(r,sysLv2FsAccess(path,amode));
*/
	return _access(path,amode,&r->_errno);
}
