////////////////////////////////////////////////////////////////////////
// PA5 - Ahmed Almutawa
// -
// Usage: ./pa5-ecfs <key phrase> <mirror directory> <mount point>
// -
// This is a standard FUSE executable that mounts to the file system
// with read, write, create behaviour supported as well as AES256 CBC
// encryption while also supporting extended attributes and regular
// filesystem operations. Built atop the fusexmp.c code.
// -
// Sources: Mikolas Szerdi & Andy Sayler | OS Concepts - Silverschatz |
// Dr. Miller slides | the people over at Stack Overflow | my mum
////////////////////////////////////////////////////////////////////////


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef linux
/* For pread()/pwrite() */
#define _XOPEN_SOURCE 500
#endif

#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif

#ifdef linux
#define ENOATTR ENODATA
#endif

#define HAVE_SETXATTR
#define USAGE "Usage: ./pa5-ecfs <key phrase> <mirror directory> <mount point>"
/* Extended attribute definitions */
#define XATRR_ENCRYPTED_FLAG "user.pa5-encfs.encrypted"
#define XATTR_FILE_PASSPHRASE "user.pa5-encfs.passphrase"
#define XMP_DATA ((struct xmp_data *) fuse_get_context()->private_data)
#define FUSE_USE_VERSION 28

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <attr/xattr.h>
#include <limits.h>
#include "aes-crypt.h"

struct xmp_data {
    	char *mirror;
    	char *key_phrase;
};

// Borrowed from Alex Beal
// https://github.com/beala/CU-CS3753-2012-PA3/blob/master/mixed.c
char* tmp_path(const char* old_path, const char *suffix){ //creates temp file
    	char* new_path;
    	int len=0;
    	len=strlen(old_path) + strlen(suffix) + 1;
    	new_path = malloc(sizeof(char)*len);
    	if(new_path == NULL){
        	return NULL;
    	}
    	new_path[0] = '\0';
    	strcat(new_path, old_path);
    	strcat(new_path, suffix);
    	return new_path;
}

static void xmp_path(char mpath[PATH_MAX], const char *path) { 
	//Routes the function paths to the mirror directory
	strcpy(mpath, XMP_DATA->mirror);
    strncat(mpath, path, PATH_MAX);
}

//-----------------------------------------------
//Following methods modified from the fusexmp.c code
//-----------------------------------------------
static int xmp_getattr(char *path, struct stat *stbuf){
	int res;
	ssize_t vsize = 0;
	char *tmpv = NULL;
	int action = -1;

	time_t    atime;   	/* time of last access */
	time_t    mtime;   	/* time of last modification */
	time_t    tctime;   /* time of last status change */
	dev_t     t_dev;    /* ID of device containing file */
	ino_t     t_ino;    /* inode number */
	mode_t    mode;    	/* protection */
	nlink_t   t_nlink;  /* number of hard links */
	uid_t     t_uid;    /* user ID of owner */
	gid_t     t_gid;    /* group ID of owner */
	dev_t     t_rdev;   /* device ID (if special file) */

	char mpath[PATH_MAX];
	xmp_path(mpath, path);

	res = lstat(mpath, stbuf);
	if (res == -1)
		return -errno;
	
	if (S_ISREG(stbuf->st_mode)){ //checks if regular file
		//following attributes don't change during decryption
		atime   = stbuf->st_atime;
		mtime   = stbuf->st_mtime;
		tctime  = stbuf->st_ctime;
		t_dev   = stbuf->st_dev;
		t_ino   = stbuf->st_ino;
		mode    = stbuf->st_mode;
		t_nlink = stbuf->st_nlink;
		t_uid   = stbuf->st_uid;
		t_gid   = stbuf->st_gid;
		t_rdev  = stbuf->st_rdev;

		/* Get size of flag value and value itself */
		vsize = getxattr(mpath, XATRR_ENCRYPTED_FLAG, NULL, 0);
		tmpv  = malloc(sizeof(*tmpv)*(vsize));
		vsize = getxattr(mpath, XATRR_ENCRYPTED_FLAG, tmpv, vsize);
		
		fprintf(stderr, "Xattr Value: %s\n", tmpv);

		if (memcmp(tmpv, "true", 4) == 0){ //if true, set flag to decrypt
			fprintf(stderr, "Encrypted, flagged to decrypt \n");
			action = 0;
		}
		else if (vsize < 0 || memcmp(tmpv, "false", 5) == 0){ //if attr empty or false, leave unencrypted
			if(errno == ENOATTR){
				fprintf(stderr, "No %s attribute set\n", XATRR_ENCRYPTED_FLAG);
			}
			fprintf(stderr, "Unencrypted, pass");
		}
		
		const char *tmpPath = tmp_path(mpath, ".getattr");
		FILE *tmpf = fopen(tmpPath, "wb+");
		FILE *f = fopen(mpath, "rb");

		fprintf(stderr, "mpath: %s\ntmpPath: %s\n", mpath, tmpPath);

		if(!do_crypt(f, tmpf, action, XMP_DATA->key_phrase)){
			fprintf(stderr, "Failed getattr do_crypt\n");
    		}

		fclose(f);
		fclose(tmpf);

		/* Get size of decrypted file and store in data struct */
		res = lstat(tmpPath, stbuf);
		if (res == -1){
			return -errno;
		}

		/* Put info about file into data struct*/
		stbuf->st_atime = atime;
		stbuf->st_mtime = mtime;
		stbuf->st_ctime = tctime;
		stbuf->st_dev = t_dev;
		stbuf->st_ino = t_ino;
		stbuf->st_mode = mode;
		stbuf->st_nlink = t_nlink;
		stbuf->st_uid = t_uid;
		stbuf->st_gid = t_gid;
		stbuf->st_rdev = t_rdev;

		free(tmpv);
		remove(tmpPath);
	}
	
	return 0;
}

static int xmp_access(char *path, int mask){
	int res;
	char mpath[PATH_MAX];
	xmp_path(mpath, path);
	res = access(mpath, mask);
	if (res == -1)
		return -errno;
	return 0;
}

static int xmp_readlink(char *path, char *buf, size_t size){
	int res;
	char mpath[PATH_MAX];
	xmp_path(mpath, path);
	res = readlink(mpath, buf, size - 1);
	if (res == -1)
		return -errno;
	buf[res] = '\0';
	return 0;
}

static int xmp_readdir(char *path, void *buf, fuse_fill_dir_t filler,
		       off_t offset, struct fuse_file_info *fi){
	DIR *dp;
	struct dirent *de;
	(void) offset;
	(void) fi;
	char mpath[PATH_MAX];
	xmp_path(mpath, path);
	dp = opendir(mpath);
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

static int xmp_mknod(char *path, mode_t mode, dev_t rdev){
	int res;
	char mpath[PATH_MAX];
	xmp_path(mpath, path);
	/* On Linux this could just be 'mknod(path, mode, rdev)' but this
	   is more portable */
	if (S_ISREG(mode)) {
		res = open(mpath, O_CREAT | O_EXCL | O_WRONLY, mode);
		if (res >= 0)
			res = close(res);
	} else if (S_ISFIFO(mode))
		res = mkfifo(mpath, mode);
	else
		res = mknod(mpath, mode, rdev);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_mkdir(char *path, mode_t mode){
	int res;
	char mpath[PATH_MAX];
	xmp_path(mpath, path);
	res = mkdir(mpath, mode);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_unlink(char *path){
	int res;
	char mpath[PATH_MAX];
	xmp_path(mpath, path);
	res = unlink(mpath);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_rmdir(char *path){
	int res;
	char mpath[PATH_MAX];
	xmp_path(mpath, path);
	res = rmdir(mpath);
	if (res == -1)
		return -errno;
	return 0;
}

static int xmp_symlink(const char *from, char *to){
	int res;
	char fto[PATH_MAX];
	xmp_path(fto, to);
	res = symlink(from, fto);
	if (res == -1)
		return -errno;
	return 0;
}

static int xmp_rename(const char *from, const char *to){
	int res;
	char ffrom[PATH_MAX];
	char fto[PATH_MAX];
	xmp_path(ffrom, from);
	xmp_path(fto, to);
	res = rename(ffrom, fto);
	if (res == -1)
		return -errno;
	return 0;
}

static int xmp_link(const char *from, const char *to){
	int res;
	char ffrom[PATH_MAX];
	char fto[PATH_MAX];
	xmp_path(ffrom, from);
	xmp_path(fto, to);
	res = link(ffrom, fto);
	if (res == -1)
		return -errno;
	return 0;
}

static int xmp_chmod(char *path, mode_t mode){
	int res;
	char mpath[PATH_MAX];
	xmp_path(mpath, path);
	res = chmod(mpath, mode);
	if (res == -1)
		return -errno;
	return 0;
}

static int xmp_chown(char *path, uid_t uid, gid_t gid){
	int res;
	char mpath[PATH_MAX];
	xmp_path(mpath, path);
	res = lchown(mpath, uid, gid);
	if (res == -1)
		return -errno;
	return 0;
}

static int xmp_truncate(char *path, off_t size){
	int res;
	char mpath[PATH_MAX];
	xmp_path(mpath, path);
	res = truncate(mpath, size);
	if (res == -1)
		return -errno;
	return 0;
}

static int xmp_utimens(char *path, const struct timespec ts[2]){
	int res;
	struct timeval tv[2];
	char mpath[PATH_MAX];
	xmp_path(mpath, path);
	tv[0].tv_sec = ts[0].tv_sec;
	tv[0].tv_usec = ts[0].tv_nsec / 1000;
	tv[1].tv_sec = ts[1].tv_sec;
	tv[1].tv_usec = ts[1].tv_nsec / 1000;
	res = utimes(mpath, tv);
	if (res == -1)
		return -errno;
	return 0;
}

static int xmp_open(char *path, struct fuse_file_info *fi){
	int res;
	char mpath[PATH_MAX];
	xmp_path(mpath, path);
	res = open(mpath, fi->flags);
	if (res == -1)
		return -errno;
	close(res);
	return 0;
}

static int xmp_read(char *path, char *buf, size_t size, off_t offset,
		    struct fuse_file_info *fi){
	(void)fi;
	int res;
	int action = -1;
	ssize_t vsize = 0;
	char *tmpv = NULL;
	char mpath[PATH_MAX];
	xmp_path(mpath, path);
	vsize = getxattr(mpath, XATRR_ENCRYPTED_FLAG, NULL, 0);
	tmpv = malloc(sizeof(*tmpv)*(vsize));
	vsize = getxattr(mpath, XATRR_ENCRYPTED_FLAG, tmpv, vsize);
	fprintf(stderr, " Read: Xattr Value: %s\n", tmpv);

	if (memcmp(tmpv, "true", 4) == 0){ // If true, encrypted and flag to decrypt
		fprintf(stderr, "Read: Encrypted, flagged to decrypt\n");
		action = 0;
	}
	else if (vsize < 0 || memcmp(tmpv, "false", 5) == 0){ //If attr doesn't exist or = to false
		if(errno == ENOATTR){
			fprintf(stderr, "Read: No %s attribute set\n", XATRR_ENCRYPTED_FLAG);
		}
		fprintf(stderr, "Read: Unencrypted, pass\n");
	}

	const char *tmpPath = tmp_path(mpath, ".read");
	FILE *tmpf = fopen(tmpPath, "wb+");
	FILE *f = fopen(mpath, "rb");
	fprintf(stderr, "Read: mpath: %s\ntmpPath: %s\n", mpath, tmpPath);

	if(!do_crypt(f, tmpf, action, XMP_DATA->key_phrase)){
		fprintf(stderr, "Read: do_crypt failed\n");
    	}

	fseek(tmpf, 0, SEEK_END);
   	size_t tmpFilelen = ftell(tmpf);
	fseek(tmpf, 0, SEEK_SET);
	fprintf(stderr, "Read: size given by read: %zu\nsize of tmpf: %zu\nsize of offset: %zu\n", size, tmpFilelen, offset);

    	/* Read the decrypted contents of original file to the application widow */
   	res = fread(buf, 1, tmpFilelen, tmpf);
	if (res == -1)
		res = -errno;

	fclose(f);
	fclose(tmpf);
	remove(tmpPath);
	free(tmpv);
	return res;
}

static int xmp_write(char *path, const char *buf, size_t size,
		     off_t offset, struct fuse_file_info *fi){
	(void) fi;
	(void) offset;
	int res;
	int fd;
	int action = -1;
	ssize_t vsize = 0;
	char *tmpv = NULL;
	char mpath[PATH_MAX];
	xmp_path(mpath, path);
	vsize = getxattr(mpath, XATRR_ENCRYPTED_FLAG, NULL, 0);
	tmpv = malloc(sizeof(*tmpv)*(vsize));
	vsize = getxattr(mpath, XATRR_ENCRYPTED_FLAG, tmpv, vsize);
	fprintf(stderr, " WRITE: Xattr Value: %s\n", tmpv);
	if (vsize < 0 || memcmp(tmpv, "false", 5) == 0){
		if(errno == ENOATTR){
			fprintf(stderr, "WRITE: No %s attribute set\n", XATRR_ENCRYPTED_FLAG);
		}
		fprintf(stderr, "WRITE: Unencrypted, pass\n\n");
	}

	/* If the attribute exists and is true get size of decrypted file */
	else if (memcmp(tmpv, "true", 4) == 0){
		fprintf(stderr, "WRITE: Encrypted, flagged to decrypt\n");
		action = 0;
	}
	/* If the file to be written to is encrypted */
	if (action == 0){
		fprintf(stderr, "WRITE: Encrypted file\n");
		
		FILE *f = fopen(mpath, "rb+");
		const char *tmpPath = tmp_path(mpath, ".write");
		FILE *tmpf = fopen(tmpPath, "wb+");

		fprintf(stderr, "Path of original file %s\n", mpath);

		fseek(f, 0, SEEK_END);
		size_t original = ftell(f);
		fseek(f, 0, SEEK_SET);
		fprintf(stderr, "Size of original file %zu\n", original);
		fprintf(stderr, "Decrypting to write\n");

		if(!do_crypt(f, tmpf, 0, XMP_DATA->key_phrase)){
			fprintf(stderr, "WRITE: do_crypt failed\n");
    	}

    	fseek(f, 0, SEEK_SET);
    	size_t tmpFilelen = ftell(tmpf);
    	fprintf(stderr, "Size to be written to tmpf %zu\n", size);
    	fprintf(stderr, "size of tmpf %zu\n", tmpFilelen);
    	fprintf(stderr, "Writing to tmpf\n");

    	res = fwrite(buf, 1, size, tmpf);
    	if (res == -1)
		res = -errno;

		tmpFilelen = ftell(tmpf);
		fprintf(stderr, "Size of tmpf after write %zu\n", tmpFilelen);

		fseek(tmpf, 0, SEEK_SET);

		fprintf(stderr, "Encrypting new contents of tmpf into original file\n");
		if(!do_crypt(tmpf, f, 1, XMP_DATA->key_phrase)){
			fprintf(stderr, "WRITE: do_crypt failed\n");
		}

		fclose(f);
		fclose(tmpf);
		remove(tmpPath);
    	
	}
	/* If the file to be written to is unencrypted */
	else if (action == -1){
		fprintf(stderr, "File to be written is unencrypted");
		fd = open(mpath, O_WRONLY);
		if (fd == -1)
			return -errno;
		res = pwrite(fd, buf, size, offset);
		if (res == -1)
			res = -errno;
		close(fd);
   	}
	free(tmpv);
	return res;
}

static int xmp_statfs(char *path, struct statvfs *stbuf){
	int res;
	char mpath[PATH_MAX];
	xmp_path(mpath, path);
	res = statvfs(mpath, stbuf);
	if (res == -1)
		return -errno;
	return 0;
}

static int xmp_create(const char* path, mode_t mode, struct fuse_file_info* fi) {
    
    char mpath[PATH_MAX];
	xmp_path(mpath, path);
	(void) fi;
	(void) mode;
	FILE *f = fopen(mpath, "wb+");

	fprintf(stderr, "CREATE: mpath: %s\n", mpath);
	if(!do_crypt(f, f, 1, XMP_DATA->key_phrase)){
		fprintf(stderr, "Create: do_crypt failed\n");
    	}
	fprintf(stderr, "Create: encryption successful\n");
	fclose(f);

	if(setxattr(mpath, XATRR_ENCRYPTED_FLAG, "true", 4, 0)){
    		fprintf(stderr, "Error: Setting xattr failed %s\n", mpath);
    	return -errno;
   	}
   	fprintf(stderr, "Create: Setting xattr successful %s\n", mpath);
    return 0;
}

static int xmp_release(char *path, struct fuse_file_info *fi){
	/* Just a stub.	 This method is optional and can safely be left
	   unimplemented */
	(void) path;
	(void) fi;
	return 0;
}

static int xmp_fsync(char *path, int isdatasync,
		     struct fuse_file_info *fi){
	/* Just a stub.	 This method is optional and can safely be left
	   unimplemented */
	(void) path;
	(void) isdatasync;
	(void) fi;
	return 0;
}
#ifdef HAVE_SETXATTR
static int xmp_setxattr(char *path, const char *name, const char *value,
			size_t size, int flags){
	char mpath[PATH_MAX];
	xmp_path(mpath, path);
	int res = lsetxattr(mpath, name, value, size, flags);
	if (res == -1)
		return -errno;
	return 0;
}

static int xmp_getxattr(char *path, const char *name, char *value, size_t size){
	char mpath[PATH_MAX];
	xmp_path(mpath, path);
	int res = lgetxattr(mpath, name, value, size);
	if (res == -1)
		return -errno;
	return res;
}

static int xmp_listxattr(char *path, char *list, size_t size){
	char mpath[PATH_MAX];
	xmp_path(mpath, path);
	int res = llistxattr(mpath, list, size);
	if (res == -1)
		return -errno;
	return res;
}

static int xmp_removexattr(char *path, const char *name){
	char mpath[PATH_MAX];
	xmp_path(mpath, path);
	int res = lremovexattr(mpath, name);
	if (res == -1)
		return -errno;
	return 0;
}
#endif /* HAVE_SETXATTR */

static struct fuse_operations xmp_oper = { //warnings checked original code by Mikolas, present there too
	.getattr     = xmp_getattr,
	.access      = xmp_access,
	.readlink    = xmp_readlink,
	.readdir     = xmp_readdir,
	.mknod       = xmp_mknod,
	.mkdir       = xmp_mkdir,
	.symlink     = xmp_symlink,
	.unlink      = xmp_unlink,
	.rmdir       = xmp_rmdir,
	.rename      = xmp_rename,
	.link        = xmp_link,
	.chmod       = xmp_chmod,
	.chown       = xmp_chown,
	.truncate    = xmp_truncate,
	.utimens     = xmp_utimens,
	.open        = xmp_open,
	.read        = xmp_read,
	.write       = xmp_write,
	.statfs      = xmp_statfs,
	.create      = xmp_create,
	.release     = xmp_release,
	.fsync       = xmp_fsync,
	#ifdef HAVE_SETXATTR
	.setxattr    = xmp_setxattr,
	.getxattr    = xmp_getxattr,
	.listxattr   = xmp_listxattr,
	.removexattr = xmp_removexattr,
	#endif
};

int main(int argc, char *argv[]){

	umask(0);
	if(argc < 4){
        	fprintf(stderr, "ERROR: Incorrect usage.\n");
        	fprintf(stderr, USAGE);
        	exit(EXIT_FAILURE);
    	}

	struct xmp_data *xmp_data;
	xmp_data = malloc(sizeof(struct xmp_data));
	if(xmp_data == NULL){
    	fprintf(stderr, "ERROR: Failed to allocate memory. Exiting.\n");
    	exit(EXIT_FAILURE);
	}
	xmp_data->mirror = realpath(argv[2], NULL); //setting mirror
	xmp_data->key_phrase = argv[1]; //setting passphrase

	fprintf(stdout, "Key Phrase = %s\n", xmp_data->key_phrase);
	fprintf(stdout, "Mirror  = %s\n", xmp_data->mirror);

	/* Passing only 3 arguments, this includes any flags such as -d, from argv to fuse_main because fuse main will
	* use these in other fuse functions: program name and mount point and optional flags
	*/
	argv[1] = argv[3];//Moving mount directory to second argument
	argv[2] = argv[4];//Moving flag to third argument
	argv[3] = NULL;
	argv[4] = NULL;
	argc = argc - 2; // making sure number of arguments matches up with size of argv

	return fuse_main(argc, argv, &xmp_oper, xmp_data);
}
