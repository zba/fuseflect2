/*
 * fuseflect - A FUSE filesystem for local directory mirroring
 *
 * Copyright (c) 2007 Theodoros V. Kalamatianos <nyb@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 *
 * Compilation:
 *
 * gcc -Wall `pkg-config fuse --cflags --libs` fuseflect.c -o fuseflect
 */



#define DEBUG		0



#define _GNU_SOURCE

#define FUSE_USE_VERSION 26

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <limits.h>
#include <pthread.h>
#include <search.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fsuid.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/xattr.h>
#include <unistd.h>



#if DEBUG
#define DBGMSG(M, ...)	fprintf(stderr, "%s:%s:%i " M "\n", __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__); fflush(stderr);
#else
#define DBGMSG(M, ...)
#endif



static char *src = "";



#define SRC(p)		char __##p [PATH_MAX + 1]; \
			strncpy(__##p , src, PATH_MAX); \
			strncat(__##p , p, PATH_MAX - strlen(src)); \
                        p = __##p ;


#define TRY(x, s, f, r) int (r) = (x); if ((r) >= 0) { s; } else { int __e = -errno; f; return __e; }
#define RET(x, s, f)	TRY(x, s, f, __r); return 0;


#define IDRET(x, s, f)	struct fuse_context *ctx = fuse_get_context(); \
			int ouid __attribute__ ((unused)) = setfsuid(ctx->uid); \
			int ogid __attribute__ ((unused)) = setfsgid(ctx->gid); \
			RET(x, \
			    s; setfsuid(ouid); setfsgid(ogid), \
			    f; setfsuid(ouid); setfsgid(ogid) \
			)



static int flect_getattr(const char *path, struct stat *stbuf)
{
	SRC(path)
	RET(lstat(path, stbuf),,)
}

static int flect_access(const char *path, int mask)
{
	SRC(path)
	RET(access(path, mask),,)
}

static int flect_readlink(const char *path, char *buf, size_t size)
{
	SRC(path)
	RET(readlink(path, buf, size - 1), buf[__r] = '\0',)
}

static int flect_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi)
{
	DIR *dp;
	struct dirent *de;

	SRC(path)

	dp = opendir(path);
	if (dp == NULL)
    		return -errno;

	while ((de = readdir(dp)) != NULL) {
    		struct stat st;
    		memset(&st, 0, sizeof(st));
    		st.st_ino = de->d_ino;
    		st.st_mode = de->d_type << 12;
    		if (filler(buf, de->d_name, &st, 0))
        		break;
	}

	closedir(dp);
	return 0;
}

static int flect_mknod(const char *path, mode_t mode, dev_t rdev)
{
	SRC(path)
	IDRET(mknod(path, mode, rdev),,)
}

static int flect_mkdir(const char *path, mode_t mode)
{
	SRC(path)
	IDRET(mkdir(path, mode),,)
}

static int flect_unlink(const char *path)
{
	SRC(path)
	RET(unlink(path),,)
}

static int flect_rmdir(const char *path)
{
	SRC(path)
	RET(rmdir(path),,)
}

static int flect_symlink(const char *from, const char *to)
{
	SRC(to)
	IDRET(symlink(from, to),,)
}

static int flect_rename(const char *from, const char *to)
{
	SRC(from)
	SRC(to)
	RET(rename(from, to),,)
}

static int flect_link(const char *from, const char *to)
{
	SRC(from)
	SRC(to)
	IDRET(link(from, to),,)
}

static int flect_chmod(const char *path, mode_t mode)
{
	SRC(path)
	RET(chmod(path, mode),,)
}

static int flect_chown(const char *path, uid_t uid, gid_t gid)
{
	SRC(path)
	RET(lchown(path, uid, gid),,)
}

static int flect_truncate(const char *path, off_t size)
{
	SRC(path)
	RET(truncate(path, size),,)
}

static int flect_utimens(const char *path, const struct timespec ts[2])
{
	struct timeval tv[2];

	tv[0].tv_sec = ts[0].tv_sec;
	tv[0].tv_usec = ts[0].tv_nsec / 1000;
	tv[1].tv_sec = ts[1].tv_sec;
	tv[1].tv_usec = ts[1].tv_nsec / 1000;

	SRC(path)
	RET(utimes(path, tv),,)
}

static int flect_open(const char *path, struct fuse_file_info *fi)
{
	SRC(path)
	RET(open(path, fi->flags), close(__r),)
}

static int flect_read(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi)
{
	SRC(path)
	TRY(open(path, O_RDONLY),,,fd)
	RET(pread(fd, buf, size, offset), close(fd); return __r, close(fd))
}

static int flect_write(const char *path, const char *buf, size_t size,
                     off_t offset, struct fuse_file_info *fi)
{
	SRC(path)
	TRY(open(path, O_WRONLY),,,fd)
	RET(pwrite(fd, buf, size, offset), close(fd); return __r, close(fd))
}

static int flect_statfs(const char *path, struct statvfs *stbuf)
{
	SRC(path)
	RET(statvfs(path, stbuf),,)
}

static int flect_setxattr(const char *path, const char *name, const char *value,
                        size_t size, int flags)
{
	SRC(path)
	RET(lsetxattr(path, name, value, size, flags),,)
}

static int flect_getxattr(const char *path, const char *name, char *value,
                    size_t size)
{
	SRC(path)
	RET(lgetxattr(path, name, value, size),,)
}

static int flect_listxattr(const char *path, char *list, size_t size)
{
	SRC(path)
	RET(llistxattr(path, list, size),,)
}

static int flect_removexattr(const char *path, const char *name)
{
	SRC(path)
	RET(lremovexattr(path, name),,)
}


#define OP(x)		. x = flect_##x ,
static struct fuse_operations flect_oper = {
	OP(getattr)
	OP(access)
	OP(readlink)
	OP(readdir)
	OP(readdir)
	OP(mknod)
	OP(mkdir)
	OP(symlink)
	OP(unlink)
	OP(rmdir)
	OP(rename)
	OP(link)
	OP(chmod)
	OP(chown)
	OP(truncate)
	OP(utimens)
	OP(open)
	OP(read)
	OP(write)
	OP(statfs)
	OP(setxattr)
	OP(getxattr)
	OP(listxattr)
	OP(removexattr)
};

int main(int argc, char *argv[])
{
	int i, cwd, c = argc;


	for (i = 1; i < (argc); ++i)
		if (strcmp("-h", argv[i]) == 0) {
			c = 2;
			break;
		}

	if (c < 3) {
		printf("\nfuseflect %s - A FUSE filesystem for local directory mirroring\n"
		       "Copyright (c) 2007 Theodoros V. Kalamatianos <nyb@users.sourceforge.net>\n\n",
		       VERSION);

		char *args[] = { NULL, "-h", };

		args[0] = malloc(strlen(argv[0]) + strlen(" sourcedir") + 1);
		if (args[0] == NULL) {
			perror("Command line argument processing failed");
			return errno;
		}

		sprintf(args[0],"%s%s", argv[0], " sourcedir");

		return fuse_main(2, args, &flect_oper, NULL);
	}


	cwd = open(".", O_RDONLY);

	i = chdir(argv[1]);
	if (i == -1) {
		perror("Cannot enter source directory");
		return errno;
	}
	src = get_current_dir_name();

	fchdir(cwd);
	close(cwd);


	char **pargv = malloc((argc - 1)* sizeof(char *));
	if (pargv == NULL) {
		perror("Command line argument processing failed");
		return errno;
	}
	pargv[0] = argv[0];
	for (i = 2; i < argc; ++i)
		pargv[i - 1] = argv[i];
		

	umask(0);
	return fuse_main(argc - 1, pargv, &flect_oper, NULL);
}
