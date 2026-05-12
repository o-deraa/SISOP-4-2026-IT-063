#define FUSE_USE_VERSION 28
#define _FILE_OFFSET_BITS 64

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <ctype.h>

static char dirpath[1000];
static char overlaypath[1000];

static int generate_tujuan(char *out, size_t maxlen)
{
    char result[8192];
    int  rlen = snprintf(result, sizeof(result), "Tujuan Mas Amba: ");

    for (int i = 1; i <= 7; i++) {
        char fpath[1000];
        snprintf(fpath, sizeof(fpath), "%s/%d.txt", dirpath, i);

        FILE *f = fopen(fpath, "r");
        if (!f) {
            fprintf(stderr, "[DEBUG] File tidak ada: %s\n", fpath);
            continue;
        }

        char line[2048];
        while (fgets(line, sizeof(line), f)) {
            char *p = strstr(line, "KOORD:");
            if (!p)
                continue;

            p += strlen("KOORD:");
            
            while (*p && isspace(*p)) p++;
            
            char *end = strchr(p, '\n');
            if (!end) end = p + strlen(p);
            
            while (end > p && isspace(*(end-1))) end--;

            size_t fraglen = (size_t)(end - p);
            if (fraglen == 0) continue;
            
            fprintf(stderr, "[DEBUG] Found fragment #%d (len=%zu): ", i, fraglen);
            fwrite(p, 1, fraglen, stderr);
            fprintf(stderr, "\n");

            if (rlen + (int)fraglen + 2 < (int)sizeof(result)) {
                memcpy(result + rlen, p, fraglen);
                rlen += (int)fraglen;
            }
        }
        fclose(f);
    }

    if (rlen < (int)sizeof(result) - 1) {
        result[rlen++] = '\n';
        result[rlen]   = '\0';
    }

    fprintf(stderr, "[DEBUG] Final result length: %d\n", rlen);
    fprintf(stderr, "[DEBUG] Final result: %s", result);

    size_t copy = (size_t)rlen < maxlen ? (size_t)rlen : maxlen;
    memcpy(out, result, copy);
    return (int)copy;
}

static inline int is_tujuan(const char *path)
{
    return strcmp(path, "/tujuan.txt") == 0;
}

static inline int tujuan_exists()
{
    char fpath[1000];
    snprintf(fpath, sizeof(fpath), "%s/tujuan.txt", overlaypath);
    return access(fpath, F_OK) == 0;
}

static int xmp_getattr(const char *path, struct stat *stbuf)
{
    if (is_tujuan(path) && tujuan_exists()) {
        char content[8192];
        int  len = generate_tujuan(content, sizeof(content));

        memset(stbuf, 0, sizeof(*stbuf));
        stbuf->st_mode  = S_IFREG | 0644;
        stbuf->st_nlink = 1;
        stbuf->st_size  = len;
        return 0;
    }

    char fpath[1000];

    snprintf(fpath, sizeof(fpath), "%s%s", overlaypath, path);
    if (lstat(fpath, stbuf) == 0)
        return 0;

    snprintf(fpath, sizeof(fpath), "%s%s", dirpath, path);
    if (lstat(fpath, stbuf) == -1)
        return -errno;

    return 0;
}

static int xmp_readdir(const char *path, void *buf,
                       fuse_fill_dir_t filler,
                       off_t offset,
                       struct fuse_file_info *fi)
{
    DIR *dp;
    struct dirent *de;
    char fpath[1000];

    (void) offset;
    (void) fi;

    snprintf(fpath, sizeof(fpath), "%s%s", dirpath, path);
    dp = opendir(fpath);

    if (dp) {
        while ((de = readdir(dp)) != NULL) {
            struct stat st;
            memset(&st, 0, sizeof(st));
            st.st_ino = de->d_ino;
            st.st_mode = de->d_type << 12;
            filler(buf, de->d_name, &st, 0);
        }
        closedir(dp);
    }

    snprintf(fpath, sizeof(fpath), "%s%s", overlaypath, path);
    dp = opendir(fpath);

    if (dp) {
        while ((de = readdir(dp)) != NULL) {
            struct stat st;
            memset(&st, 0, sizeof(st));
            st.st_ino = de->d_ino;
            st.st_mode = de->d_type << 12;
            filler(buf, de->d_name, &st, 0);
        }
        closedir(dp);
    }

    return 0;
}

static int xmp_open(const char *path, struct fuse_file_info *fi)
{
    if (is_tujuan(path) && tujuan_exists())
        return 0;

    char fpath[1000];
    snprintf(fpath, sizeof(fpath), "%s%s", dirpath, path);

    int fd = open(fpath, fi->flags);
    if (fd == -1)
        return -errno;

    close(fd);
    return 0;
}

static int xmp_read(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi)
{
    if (is_tujuan(path) && tujuan_exists()) {
        char content[8192];
        int  len = generate_tujuan(content, sizeof(content));

        if (offset >= len)
            return 0;

        size_t avail = (size_t)(len - offset);
        size_t copy  = avail < size ? avail : size;
        memcpy(buf, content + offset, copy);
        return (int)copy;
    }

    char fpath[1000];
    snprintf(fpath, sizeof(fpath), "%s%s", dirpath, path);

    int fd = open(fpath, O_RDONLY);
    if (fd == -1)
        return -errno;

    int res = pread(fd, buf, size, offset);
    if (res == -1)
        res = -errno;

    close(fd);
    return res;
}

static int xmp_write(const char *path, const char *buf, size_t size, off_t offset,
                     struct fuse_file_info *fi)
{
    char fpath[1000];
    snprintf(fpath, sizeof(fpath), "%s%s", overlaypath, path);

    int fd = open(fpath, O_WRONLY | O_CREAT, 0644);
    if (fd == -1)
        return -errno;

    int res = pwrite(fd, buf, size, offset);
    if (res == -1)
        res = -errno;

    close(fd);
    return res;
}

static int xmp_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    char fpath[1000];
    snprintf(fpath, sizeof(fpath), "%s%s", overlaypath, path);

    int fd = open(fpath, O_CREAT | O_WRONLY | O_TRUNC, mode);
    if (fd == -1)
        return -errno;

    fi->fh = fd;
    return 0;
}

static int xmp_unlink(const char *path)
{
    char fpath[1000];
    snprintf(fpath, sizeof(fpath), "%s%s", overlaypath, path);

    if (unlink(fpath) == -1)
        return -errno;

    return 0;
}

static int xmp_utimens(const char *path, const struct timespec ts[2])
{
    char fpath[1000];

    snprintf(fpath, sizeof(fpath), "%s%s", overlaypath, path);
    if (access(fpath, F_OK) == 0) {
        if (utimensat(0, fpath, ts, AT_SYMLINK_NOFOLLOW) == -1)
            return -errno;
        return 0;
    }

    snprintf(fpath, sizeof(fpath), "%s%s", dirpath, path);
    if (utimensat(0, fpath, ts, AT_SYMLINK_NOFOLLOW) == -1)
        return -errno;

    return 0;
}

static struct fuse_operations xmp_oper = {
    .getattr = xmp_getattr,
    .readdir = xmp_readdir,
    .read    = xmp_read,
    .write   = xmp_write,
    .create  = xmp_create,
    .open    = xmp_open,
    .unlink  = xmp_unlink,
    .utimens = xmp_utimens,
};

int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "Usage: %s [source] [destination]\n", argv[0]);
        return 1;
    }

    realpath(argv[1], dirpath);
    fprintf(stderr, "[INFO] Source directory: %s\n", dirpath);

    snprintf(overlaypath, sizeof(overlaypath), "/tmp/fuse_overlay_%d", getpid());
    mkdir(overlaypath, 0700);
    fprintf(stderr, "[INFO] Overlay directory: %s\n", overlaypath);

    struct stat st = {0};
    if (stat(argv[2], &st) == -1)
        mkdir(argv[2], 0755);

    argv[1] = argv[2];
    argc = 2;

    return fuse_main(argc, argv, &xmp_oper, NULL);
}