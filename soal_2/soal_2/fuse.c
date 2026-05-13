#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <sys/xattr.h>
#include <sys/time.h>

#define ENCRYPTION_KEY 0x76
#define MAX_PATH 4096

// Structure untuk menyimpan path encrypted_storage
typedef struct {
    char *encrypted_dir;
} fuse_context_t;

// Fungsi untuk melakukan XOR encryption/decryption
void xor_crypt(unsigned char *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        data[i] ^= ENCRYPTION_KEY;
    }
}

// Fungsi untuk membaca file terenkripsi dan decrypt
int read_encrypted_file(const char *path, unsigned char *buf, size_t size, off_t offset) {
    FILE *f = fopen(path, "rb");
    if (!f) return -errno;

    fseek(f, offset, SEEK_SET);
    int nread = fread(buf, 1, size, f);
    fclose(f);

    if (nread > 0) {
        xor_crypt(buf, nread);
    }

    return nread;
}

// Fungsi untuk menulis dan encrypt file
int write_encrypted_file(const char *path, const unsigned char *buf, size_t size, off_t offset) {
    FILE *f = fopen(path, "r+b");
    if (!f) {
        f = fopen(path, "wb");
        if (!f) return -errno;
    }

    unsigned char *encrypted_buf = malloc(size);
    if (!encrypted_buf) {
        fclose(f);
        return -ENOMEM;
    }

    memcpy(encrypted_buf, buf, size);
    xor_crypt(encrypted_buf, size);

    fseek(f, offset, SEEK_SET);
    int nwritten = fwrite(encrypted_buf, 1, size, f);
    fclose(f);
    free(encrypted_buf);

    return nwritten > 0 ? nwritten : -errno;
}

static void get_encrypted_path(const char *fuse_path, char *encrypted_path, const char *encrypted_dir) {
    if (strcmp(fuse_path, "/") == 0) {
        strcpy(encrypted_path, encrypted_dir);
        return;
    }
 
    // Check if path exists as directory (without .enc)
    // If yes, use it as-is. If no, treat as file and add .enc
    struct stat st;
    char temp_path[MAX_PATH];
    snprintf(temp_path, MAX_PATH, "%s%s", encrypted_dir, fuse_path);
    
    // If path exists as directory, use without .enc
    if (stat(temp_path, &st) == 0 && S_ISDIR(st.st_mode)) {
        strcpy(encrypted_path, temp_path);
        return;
    }
    
    // Otherwise, treat as file and add .enc
    snprintf(encrypted_path, MAX_PATH, "%s%s.enc", encrypted_dir, fuse_path);
}

// Mendapatkan encrypted filename (for files)
// For directories, just append path without .enc
// For directories
static void get_encrypted_dir_path(const char *fuse_path, char *encrypted_path, const char *encrypted_dir) {
    if (strcmp(fuse_path, "/") == 0) {
        strcpy(encrypted_path, encrypted_dir);
        return;
    }
    snprintf(encrypted_path, MAX_PATH, "%s%s", encrypted_dir, fuse_path);
    // NO .enc suffix for directories!
}

// For files
static void get_encrypted_file_path(const char *fuse_path, char *encrypted_path, const char *encrypted_dir) {
    if (strcmp(fuse_path, "/") == 0) {
        strcpy(encrypted_path, encrypted_dir);
        return;
    }
    snprintf(encrypted_path, MAX_PATH, "%s%s.enc", encrypted_dir, fuse_path);
    // .enc suffix for files
}

// Mendapatkan decrypted filename dari encrypted
static void get_decrypted_filename(const char *encrypted_name, char *decrypted_name) {
    strcpy(decrypted_name, encrypted_name);
    
    // Hapus .enc suffix jika ada
    size_t len = strlen(decrypted_name);
    if (len > 4 && strcmp(&decrypted_name[len-4], ".enc") == 0) {
        decrypted_name[len-4] = '\0';
    }
}

// FUSE Callback Functions
static int moo_getattr(const char *path, struct stat *stbuf) {
    fuse_context_t *ctx = (fuse_context_t *)fuse_get_context()->private_data;
    char encrypted_path[MAX_PATH];
    get_encrypted_path(path, encrypted_path, ctx->encrypted_dir);

    int res = stat(encrypted_path, stbuf);
    if (res == -1)
        return -errno;

    return 0;
}

static int moo_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
    (void)offset;
    (void)fi;

    fuse_context_t *ctx = (fuse_context_t *)fuse_get_context()->private_data;
    char encrypted_path[MAX_PATH];
    get_encrypted_dir_path(path, encrypted_path, ctx->encrypted_dir);

    DIR *dp = opendir(encrypted_path);
    if (!dp)
        return -errno;

    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;

        char decrypted_name[256];
        get_decrypted_filename(de->d_name, decrypted_name);

        if (filler(buf, decrypted_name, &st, 0))
            break;
    }

    closedir(dp);
    return 0;
}

static int moo_mkdir(const char *path, mode_t mode) {
    fuse_context_t *ctx = (fuse_context_t *)fuse_get_context()->private_data;
    char encrypted_path[MAX_PATH];
    get_encrypted_dir_path(path, encrypted_path, ctx->encrypted_dir);

    int res = mkdir(encrypted_path, mode);
    if (res == -1)
        return -errno;

    return 0;
}

static int moo_rmdir(const char *path) {
    fuse_context_t *ctx = (fuse_context_t *)fuse_get_context()->private_data;
    char encrypted_path[MAX_PATH];
    get_encrypted_dir_path(path, encrypted_path, ctx->encrypted_dir);

    int res = rmdir(encrypted_path);
    if (res == -1)
        return -errno;

    return 0;
}

static int moo_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    fuse_context_t *ctx = (fuse_context_t *)fuse_get_context()->private_data;
    char encrypted_path[MAX_PATH];
    get_encrypted_file_path(path, encrypted_path, ctx->encrypted_dir);

    int fd = creat(encrypted_path, mode);
    if (fd == -1)
        return -errno;

    fi->fh = fd;
    return 0;
}

static int moo_open(const char *path, struct fuse_file_info *fi) {
    fuse_context_t *ctx = (fuse_context_t *)fuse_get_context()->private_data;
    char encrypted_path[MAX_PATH];
    get_encrypted_file_path(path, encrypted_path, ctx->encrypted_dir);

    int fd = open(encrypted_path, fi->flags);
    if (fd == -1)
        return -errno;

    fi->fh = fd;
    return 0;
}

static int moo_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    fuse_context_t *ctx = (fuse_context_t *)fuse_get_context()->private_data;
    char encrypted_path[MAX_PATH];
    get_encrypted_file_path(path, encrypted_path, ctx->encrypted_dir);

    int nread = read_encrypted_file(encrypted_path, (unsigned char *)buf, size, offset);
    return nread;
}

static int moo_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    fuse_context_t *ctx = (fuse_context_t *)fuse_get_context()->private_data;
    char encrypted_path[MAX_PATH];
    get_encrypted_file_path(path, encrypted_path, ctx->encrypted_dir);

    int nwritten = write_encrypted_file(encrypted_path, (unsigned char *)buf, size, offset);
    return nwritten;
}

static int moo_truncate(const char *path, off_t size) {
    fuse_context_t *ctx = (fuse_context_t *)fuse_get_context()->private_data;
    char encrypted_path[MAX_PATH];
    get_encrypted_path(path, encrypted_path, ctx->encrypted_dir);

    int res = truncate(encrypted_path, size);
    if (res == -1)
        return -errno;

    return 0;
}

static int moo_unlink(const char *path) {
    fuse_context_t *ctx = (fuse_context_t *)fuse_get_context()->private_data;
    char encrypted_path[MAX_PATH];
    get_encrypted_path(path, encrypted_path, ctx->encrypted_dir);

    int res = unlink(encrypted_path);
    if (res == -1)
        return -errno;

    return 0;
}

static int moo_access(const char *path, int mask) {
    fuse_context_t *ctx = (fuse_context_t *)fuse_get_context()->private_data;
    char encrypted_path[MAX_PATH];
    get_encrypted_path(path, encrypted_path, ctx->encrypted_dir);

    int res = access(encrypted_path, mask);
    if (res == -1)
        return -errno;

    return 0;
}

static int moo_utimens(const char *path, const struct timespec ts[2]) {
    fuse_context_t *ctx = (fuse_context_t *)fuse_get_context()->private_data;
    char encrypted_path[MAX_PATH];
    get_encrypted_path(path, encrypted_path, ctx->encrypted_dir);

    // For FUSE 2.9, use utimensat
    int res = utimensat(AT_FDCWD, encrypted_path, ts, AT_SYMLINK_NOFOLLOW);
    if (res == -1) {
        // Fallback to utime for older systems
        struct timeval times[2];
        times[0].tv_sec = ts[0].tv_sec;
        times[0].tv_usec = ts[0].tv_nsec / 1000;
        times[1].tv_sec = ts[1].tv_sec;
        times[1].tv_usec = ts[1].tv_nsec / 1000;
        
        res = utimes(encrypted_path, times);
        if (res == -1)
            return -errno;
    }

    return 0;
}

static int moo_flush(const char *path, struct fuse_file_info *fi) {
    (void)path;
    if (fi->fh) {
        // Flush the file descriptor
        fsync(fi->fh);
    }
    return 0;
}

static struct fuse_operations moo_oper = {
    .getattr = moo_getattr,
    .readdir = moo_readdir,
    .mkdir = moo_mkdir,
    .rmdir = moo_rmdir,
    .create = moo_create,
    .open = moo_open,
    .read = moo_read,
    .write = moo_write,
    .truncate = moo_truncate,
    .unlink = moo_unlink,
    .access = moo_access,
    .utimens = moo_utimens,
    .flush = moo_flush,
};

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <encrypted_dir> <mount_point> [FUSE options]\n", argv[0]);
        fprintf(stderr, "Example: %s ./encrypted_storage ./fuse_mount -f\n", argv[0]);
        return 1;
    }

    fuse_context_t ctx;
    ctx.encrypted_dir = realpath(argv[1], NULL);
    if (!ctx.encrypted_dir) {
        perror("realpath");
        return 1;
    }

    char *mount_point = argv[2];

    // Build new argv array untuk FUSE
    // FUSE expect: argv[0]=program, argv[1]=mount_point, argv[2...]=options
    for (int i = 1; i < argc - 1; i++) {
        argv[i] = argv[i + 1];
    }
    argc--;

    return fuse_main(argc, argv, &moo_oper, &ctx);;
}