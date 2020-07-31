
#include <ansilove.h>
#include <magic.h>
#include <stdio.h>
#define MIME_DB "/usr/share/file/magic.mgc"
#define FUSE_USE_VERSION 26

static const char* rofsVersion = "2008.09.24";

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <stdio.h>
#include <strings.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/xattr.h>
#include <dirent.h>
#include <unistd.h>
#include <fuse.h>

// Global to store our read-write path
char *rw_path;

//Check if file has .png extension 
int is_png(const char * file){
	char* a = strrchr(file, '.');
	if (a == NULL) return 0;
	return ((strcmp(a, ".png") == 0) ? 1 : 0); 
}

//Compare two file paths without taking extensions and directories in consideration
int smart_cmp(char * a, char * b) {
	char * start_a = strrchr(a, '/');
	if (start_a == NULL)
		start_a = a;
	else {
		if (start_a[0] == '/') {
			start_a++;
		}
	}
	char * end_a = strrchr(a, '.');
	int len_a = 0;
	if (end_a == NULL)
		len_a = strlen(a);
		
	len_a = end_a - start_a;
	char * start_b = strrchr(b, '/');
	if (start_b == NULL)
		start_b = b;
	else {
		if (start_b[0] == '/') {
			start_b++;
		}
	}
	char * end_b = strrchr(b, '.');
	
	int len_b = 0;
	if (end_b == NULL)
		len_b = strlen(b);
	len_b = end_b - start_b;
	if (len_a != len_b) return 1;
	return strncmp(start_a, start_b, len_a - 1);
}

//Find the source of the requested file
char * mapPath(const char* path) {
	DIR *dp;
    struct dirent *de;
    char* return_path;
    char* file_path = (char*)malloc(strlen(rw_path)
						+ strlen(path) + 1);
    strcpy(file_path, rw_path);
    strcat(file_path, path);
    char * last_slash = strrchr(file_path, '/');
    printf("file_path: %s \n",file_path);
    char* directory_path = (char*)malloc(last_slash - file_path + 1);
    strncpy(directory_path, file_path, last_slash - file_path);
    directory_path[last_slash - file_path] = '\0';
    printf("directory: %s \n", directory_path);
    
    dp = opendir(directory_path);
    while((de = readdir(dp)) != NULL){
		if(smart_cmp(de->d_name, path) == 0){
			break;
		} 
	}
	if (de == NULL)
		return NULL;
	return_path = (char *)malloc(strlen(directory_path)
					+ strlen(de->d_name) + 2);
	strcpy(return_path, directory_path);
	strcat(return_path, "/");
	strcat(return_path, de->d_name);
	free(file_path);
	free(directory_path);
	closedir(dp);
	printf("real_path: %s\n",return_path); 
	return return_path;
	
}

//Convert extension to png
char * convert2png(char * file, char* filename){
	strcpy(filename, file);	
	char* a = strrchr(filename, '.');
	if (a != NULL){
		strcpy(a, ".png");
	} else {
		strcat(filename, ".png");
	}
	return filename;
}




int magic(char *file, char *mime)
{
	magic_t magic_cookie = NULL;
	magic_cookie = magic_open(MAGIC_MIME_TYPE);
	if(magic_cookie == NULL)
	{
		printf("Error creating magic cookie\n");
		return 1;
	}
	magic_load(magic_cookie,MIME_DB);
	strcpy(mime, magic_file(magic_cookie,file));
	
	magic_close(magic_cookie);
	return 0;
}





// Translate an rofs path into it's underlying filesystem path
static char* translate_path(const char* path)
{

    char *rPath= malloc(sizeof(char)*(strlen(path)+strlen(rw_path)+1));

    strcpy(rPath,rw_path);
    if (rPath[strlen(rPath)-1]=='/') {
        rPath[strlen(rPath)-1]='\0';
    }
    strcat(rPath,path);

    return rPath;
}


/******************************
*
* Callbacks for FUSE
*
*
*
******************************/

static int rofs_getattr(const char *path, struct stat *st_data)
{
	
    DIR *dp;
    struct dirent *de;
    int res;
    
	struct ansilove_ctx ctx;
	struct ansilove_options options;
    
    //Find the corresponding file in the src (path has .png extension but it can have any type of extension in src)
	if(is_png(path)) {
		//for determining the future(png version) size of the file
		char * real_path = mapPath(path);
		ansilove_init(&ctx, &options);
		ansilove_loadfile(&ctx, real_path);
		ansilove_ansi(&ctx, &options);
		res = lstat(real_path , st_data);
		st_data->st_size = ctx.png.length; //Set expected size as st_size so that fuse operates correctly
		ansilove_clean(&ctx);
		free(real_path);
	} 
	//Do whatever you do as rofs if the path is a directory
	else {
		char *upath=translate_path(path);
		
		res = lstat(upath, st_data);
		free(upath);
	}
	
    if(res == -1) {
        return -errno;
    }
    return 0;
}

static int rofs_readlink(const char *path, char *buf, size_t size)
{
    int res;
    char *upath=translate_path(path);

    res = readlink(upath, buf, size - 1);
    free(upath);
    if(res == -1) {
        return -errno;
    }
    buf[res] = '\0';
    return 0;
}


static int rofs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,off_t offset, struct fuse_file_info *fi)
{
    DIR *dp;
    struct dirent *de;
	char  *mimetype =(char *)malloc(300);
	char *temp = (char*)malloc(300);
	char *filename = (char*)malloc(200);
    int res;

    (void) offset;
    (void) fi;

    char *upath=translate_path(path);

    dp = opendir(upath);
    
    if(dp == NULL) {
        res = -errno;
        return res;
    }

    while((de = readdir(dp)) != NULL) {
		//Check if directory, if true skip magic operations
		if(S_ISDIR(de->d_type << 12)){
			goto SEND;
		}
		//Compose the valid path of a file
		//source + / + filename
		strcpy(temp, upath);
		strcat(temp , "/");
		strcat(temp, de->d_name);
		magic(temp, mimetype);
		if(strcmp("application/octet-stream",mimetype) == 0 || strncmp(mimetype, "text", 4) == 0 ){
			SEND:;
			struct stat st;
			memset(&st, 0, sizeof(st));
			st.st_ino = de->d_ino;
			st.st_mode = de->d_type << 12;
			if(S_ISDIR(de->d_type << 12)){
				if (filler(buf, de->d_name, &st, 0))
					break;
			}else {
				convert2png(de->d_name, filename);
				if (filler(buf, filename, &st, 0)){
					break;
				}	
			}
			
		}
      
    }
    free(filename);
    free(upath);
    free(temp);
    free(mimetype);
    closedir(dp);
    return 0;
}
static int rofs_mknod(const char *path, mode_t mode, dev_t rdev)
{
    (void)path;
    (void)mode;
    (void)rdev;
    return -EROFS;
}

static int rofs_mkdir(const char *path, mode_t mode)
{
    (void)path;
    (void)mode;
    return -EROFS;
}

static int rofs_unlink(const char *path)
{
    (void)path;
    return -EROFS;
}

static int rofs_rmdir(const char *path)
{
    (void)path;
    return -EROFS;
}

static int rofs_symlink(const char *from, const char *to)
{
    (void)from;
    (void)to;
    return -EROFS;
}

static int rofs_rename(const char *from, const char *to)
{
    (void)from;
    (void)to;
    return -EROFS;
}

static int rofs_link(const char *from, const char *to)
{
    (void)from;
    (void)to;
    return -EROFS;
}

static int rofs_chmod(const char *path, mode_t mode)
{
    (void)path;
    (void)mode;
    return -EROFS;

}

static int rofs_chown(const char *path, uid_t uid, gid_t gid)
{
    (void)path;
    (void)uid;
    (void)gid;
    return -EROFS;
}

static int rofs_truncate(const char *path, off_t size)
{
    (void)path;
    (void)size;
    return -EROFS;
}

static int rofs_utime(const char *path, struct utimbuf *buf)
{
    (void)path;
    (void)buf;
    return -EROFS;
}

static int rofs_open(const char *path, struct fuse_file_info *finfo)
{
    int res;
    
    int flags = finfo->flags;

    char * real_path = mapPath(path);
	printf("OPEN: %s\n", real_path);
    res = open(real_path, flags);

    free(real_path);
    if(res == -1) {
        return -errno;
    }
    close(res);
    return 0;
}

static int rofs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *finfo)
{
	char * temp_file = "/tmp/temp_img_for_ansilove.png";
	
    int fd;
    int res;
    (void)finfo;
	int flags = finfo->flags;
    char * real_path = mapPath(path);
    
    //create ansilove version of the text file and write to temp directory
    //to correctly format the file
	struct ansilove_ctx ctx;
	struct ansilove_options options;

	ansilove_init(&ctx, &options);

	ansilove_loadfile(&ctx, real_path);

	ansilove_ansi(&ctx, &options);

	ansilove_savefile(&ctx, temp_file);

	
    fd = open(temp_file, flags);
    

    if(fd == -1) {
        res = -errno;
        return res;
    }
    
    struct stat s;
    fstat(fd, &s);
    
	res = pread(fd, buf, size, offset);

    if(res == -1) {
        res = -errno;
    }
    
    //Clean everything and delete from the temp
    free(real_path);
    ansilove_clean(&ctx);
    close(fd);
    remove(temp_file);
    return res;
}

static int rofs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *finfo)
{
    (void)path;
    (void)buf;
    (void)size;
    (void)offset;
    (void)finfo;
    return -EROFS;
}

static int rofs_statfs(const char *path, struct statvfs *st_buf)
{
    int res;
    char *upath=translate_path(path);

    res = statvfs(upath, st_buf);
    free(upath);
    if (res == -1) {
        return -errno;
    }
    return 0;
}

static int rofs_release(const char *path, struct fuse_file_info *finfo)
{
    (void) path;
    (void) finfo;
    return 0;
}

static int rofs_fsync(const char *path, int crap, struct fuse_file_info *finfo)
{
    (void) path;
    (void) crap;
    (void) finfo;
    return 0;
}

static int rofs_access(const char *path, int mode)
{
    int res;
    char *upath=translate_path(path);

    if (mode & W_OK)
        return -EROFS;

    res = access(upath, mode);
    free(upath);
    if (res == -1) {
        return -errno;
    }
    return res;
}

/*
 * Set the value of an extended attribute
 */
static int rofs_setxattr(const char *path, const char *name, const char *value, size_t size, int flags)
{
    (void)path;
    (void)name;
    (void)value;
    (void)size;
    (void)flags;
    return -EROFS;
}

/*
 * Get the value of an extended attribute.
 */
static int rofs_getxattr(const char *path, const char *name, char *value, size_t size)
{
    int res;

    char *upath=translate_path(path);
    res = lgetxattr(upath, name, value, size);
    free(upath);
    if(res == -1) {
        return -errno;
    }
    return res;
}

/*
 * List the supported extended attributes.
 */
static int rofs_listxattr(const char *path, char *list, size_t size)
{
    int res;

    char *upath=translate_path(path);
    res = llistxattr(upath, list, size);
    free(upath);
    if(res == -1) {
        return -errno;
    }
    return res;

}

/*
 * Remove an extended attribute.
 */
static int rofs_removexattr(const char *path, const char *name)
{
    (void)path;
    (void)name;
    return -EROFS;

}

struct fuse_operations rofs_oper = {
    .getattr     = rofs_getattr,
    .readlink    = rofs_readlink,
    .readdir     = rofs_readdir,
    .mknod       = rofs_mknod,
    .mkdir       = rofs_mkdir,
    .symlink     = rofs_symlink,
    .unlink      = rofs_unlink,
    .rmdir       = rofs_rmdir,
    .rename      = rofs_rename,
    .link        = rofs_link,
    .chmod       = rofs_chmod,
    .chown       = rofs_chown,
    .truncate    = rofs_truncate,
    .utime       = rofs_utime,
    .open        = rofs_open,
    .read        = rofs_read,
    .write       = rofs_write,
    .statfs      = rofs_statfs,
    .release     = rofs_release,
    .fsync       = rofs_fsync,
    .access      = rofs_access,

    /* Extended attributes support for userland interaction */
    .setxattr    = rofs_setxattr,
    .getxattr    = rofs_getxattr,
    .listxattr   = rofs_listxattr,
    .removexattr = rofs_removexattr
};
enum {
    KEY_HELP,
    KEY_VERSION,
};

static void usage(const char* progname)
{
    fprintf(stdout,
            "usage: %s readwritepath mountpoint [options]\n"
            "\n"
            "   Mounts readwritepath as a read-only mount at mountpoint\n"
            "\n"
            "general options:\n"
            "   -o opt,[opt...]     mount options\n"
            "   -h  --help          print help\n"
            "   -V  --version       print version\n"
            "\n", progname);
}

static int rofs_parse_opt(void *data, const char *arg, int key,
                          struct fuse_args *outargs)
{
    (void) data;

    switch (key)
    {
    case FUSE_OPT_KEY_NONOPT:
        if (rw_path == 0)
        {
            rw_path = strdup(arg);
            return 0;
        }
        else
        {
            return 1;
        }
    case FUSE_OPT_KEY_OPT:
        return 1;
    case KEY_HELP:
        usage(outargs->argv[0]);
        exit(0);
    case KEY_VERSION:
        fprintf(stdout, "ROFS version %s\n", rofsVersion);
        exit(0);
    default:
        fprintf(stderr, "see `%s -h' for usage\n", outargs->argv[0]);
        exit(1);
    }
    return 1;
}

static struct fuse_opt rofs_opts[] = {
    FUSE_OPT_KEY("-h",          KEY_HELP),
    FUSE_OPT_KEY("--help",      KEY_HELP),
    FUSE_OPT_KEY("-V",          KEY_VERSION),
    FUSE_OPT_KEY("--version",   KEY_VERSION),
    FUSE_OPT_END
};

int main(int argc, char *argv[])
{
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    int res;

    res = fuse_opt_parse(&args, &rw_path, rofs_opts, rofs_parse_opt);
    if (res != 0)
    {
        fprintf(stderr, "Invalid arguments\n");
        fprintf(stderr, "see `%s -h' for usage\n", argv[0]);
        exit(1);
    }
    if (rw_path == 0)
    {
        fprintf(stderr, "Missing readwritepath\n");
        fprintf(stderr, "see `%s -h' for usage\n", argv[0]);
        exit(1);
    }

    fuse_main(args.argc, args.argv, &rofs_oper, NULL);

    return 0;
}
