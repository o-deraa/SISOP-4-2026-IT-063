# SISOP-4-2026-IT-063

## Dewa Ngakan Gede Wira Adhimukti - 5027251063

## SOAL 1 - Save Asisten Kenz

### a. Ambil Arsip
Pada soal ini, hal pertama yang harus dilakukan adalah mengambil arsip yang tertera, hal tersebut dapat dilakukan dengan menggunakan `gdown`.

```
gdown https://drive.google.com/file/d/1nLXFhptDo2mnUlZsw8pTWyAVpV49W20U/view?usp=drive_link
```

Setelah file berhasil didownload, file tersebut harus di unzip dan kemudian file zip harus dihapus sehingga hanya menyisakan folder hasil unzip.

- Unzip file
```
unzip amba_files.zip
```

![alt text](image.png)

- Hapus file
```
rm -rf amba_files.zip
```

![alt text](image-1.png)

### b. Buat Program FUSE
Selanjutnyua, soal meminta untuk membuat program FUSE bernama `kenz_rescue.c`. Program tersebut akan menerima dua argumen yaitu <source_directory> dan <mount_directory>. Saat program di-mount, ketujuh file 1.txt sampai 7.txt harus muncul di mount directory dengan isi sama persis dengan source-passthrough untuk callback, readdir, open, dan read (mount directory hanya bertindak sebagai filesystem cermin yang hanya meneruskan operasi ke source)

```c
int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "Usage: %s [source] [destination]\n", argv[0]);
        return 1;
    }

    realpath(argv[1], dirpath);  // Convert ke absolute path
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
```

Fungsi `main()`berperan sebagai titik awal program FUSE. Program ini membutuhkan dua argumen, yaitu source directory dan mount directory. Source directory adalah folder asli yang akan dijadikan sumber data, sedangkan destination atau mount directory adalah folder tempat filesystem FUSE akan dipasang. Karena itu, program pertama-tama mengecek apakah jumlah argumen adalah 3, yaitu nama program, source, dan destination. Jika jumlah argumen tidak sesuai, program akan menampilkan format penggunaan yang benar lalu berhenti.

Setelah argumen valid, program memanggil `realpath(argv[1], dirpath)`. Fungsi ini digunakan untuk mengubah path source yang diberikan user menjadi absolute path. Hasil absolute path ini kemudian disimpan ke variabel global `dirpath`, sehingga seluruh fungsi FUSE lain dapat mengakses lokasi source directory dengan jelas dan konsisten.

Selanjutnya, program membuat sebuah directory overlay sementara di dalam `/tmp`. Nama folder overlay dibuat unik menggunakan PID dari proses yang sedang berjalan, yaitu dengan format `/tmp/fuse_overlay_<PID>`. Directory ini disimpan dalam variabel `overlaypath`. Tujuan overlay ini adalah untuk menyimpan file-file hasil perubahan atau penulisan dari user, tanpa langsung mengubah isi directory sumber utama. Hal ini membuat source directory bisa tetap aman, sementara perubahan disimpan di tempat terpisah.

Setelah itu, program mengecek apakah mount directory yang diberikan melalui `argv[2]` sudah ada atau belum. Pengecekan dilakukan menggunakan `stat()`. Jika folder mount tersebut belum ada, maka program akan membuatnya secara otomatis dengan permission `0755`. Folder inilah yang nantinya akan menjadi tempat user melihat dan berinteraksi dengan filesystem FUSE.

 Awalnya program menerima dua argumen dari user, yaitu source dan destination. Namun, `fuse_main()` hanya perlu mengetahui mount path sebagai argumen utama FUSE. Karena itu, `argv[1]` diganti menjadi `argv[2]`, lalu `argc` diubah menjadi `2`. Dengan begitu, argumen yang diteruskan ke FUSE hanya berisi nama program dan mount directory.

Selanjutnya, program memanggil `fuse_main(argc, argv, &xmp_oper, NULL)`. Fungsi ini menjalankan FUSE dan menghubungkan filesystem dengan kumpulan callback operation yang ada di `xmp_oper`, seperti operasi membaca file, menulis file, membuka directory, menghapus file, dan lain-lain. Setelah fungsi ini berjalan, filesystem akan aktif di mount path, dan setiap operasi yang dilakukan user di folder tersebut akan diteruskan ke fungsi-fungsi callback yang sudah didefinisikan.

#### HASIL AKHIR:
![alt text](image-5.png)

![alt text](image-6.png)

### c. Buat File Virtual
Soal lalu meminta untuk menambahkan satu file virtual bernama tujuan.txt di root mount directory. File ini harus muncul di `ls mnt/`, ukurannya konsisten saat di-stat, tapi tidak boleh punya kesamaan fisik di amba_files/.

- Cek path
```c
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
```
Fungsi `is_tujuan(path)` berfungsi untuk mengecek apakah path yang sedang diakses adalah `/tujuan.txt`, sehingga program bisa mengenali bahwa operasi tersebut ditujukan ke file khusus tersebut. Fungsi `tujuan_exists()` digunakan untuk mengecek apakah file `tujuan.txt` sudah benar-benar ada di dalam overlay directory, yaitu folder sementara yang sebelumnya dibuat di `/tmp/fuse_overlay_<PID>/`. Di dalam fungsi ini, path lengkap menuju file `tujuan.txt` dibentuk menggunakan `snprintf()`, lalu dicek dengan `access(fpath, F_OK)`. Jika file tersebut ada, fungsi akan mengembalikan nilai `1` atau true, sedangkan jika tidak ada akan mengembalikan `0` atau false. Jadi, ketika menjalankan perintah seperti `touch /mount/tujuan.txt`, file kosong akan dibuat di overlay directory, dan fungsi ini dapat digunakan untuk memeriksa apakah file tersebut sudah tersedia atau belum.

- Ukuran konsisten

```c
static int xmp_getattr(const char *path, struct stat *stbuf)
{
    if (is_tujuan(path) && tujuan_exists()) {
        char content[8192];
        int  len = generate_tujuan(content, sizeof(content));

        memset(stbuf, 0, sizeof(*stbuf));
        stbuf->st_mode  = S_IFREG | 0644;  // regular file, rw-r--r--
        stbuf->st_nlink = 1;
        stbuf->st_size  = len; // size = panjang konten virtual
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
```
Fungsi `xmp_getattr()` digunakan untuk mengambil metadata sebuah file atau folder di filesystem FUSE, misalnya ketika user menjalankan `ls -l`, `stat`, atau ketika sistem perlu mengetahui ukuran dan permission sebuah file. Pada soal ini, fungsi tersebut terlebih dahulu mengecek apakah path yang diakses adalah "/tujuan.txt" dan apakah file tersebut sudah ada di overlay directory. Jika iya, maka `generate_tujuan()` dipanggil untuk menghitung panjang konten virtual yang akan ditampilkan. Setelah itu, `stbuf` diisi manual dengan metadata `file: st_mode` menandakan bahwa `tujuan.txt` adalah regular file dengan `permission 0644`, `st_nlink` bernilai 1, dan `st_size` diisi berdasarkan panjang konten hasil generate, sehingga ukuran file tidak dianggap kosong meskipun isi sebenarnya dibangkitkan secara virtual.

- Buat file
```c
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
```

Fungsi `xmp_create()` digunakan ketika user membuat file baru di mount FUSE, misalnya saat menjalankan `touch /mount/tujuan.txt`. Di dalam fungsi ini, program terlebih dahulu membentuk path lengkap file di overlay directory dengan menggabungkan `overlaypath` dan `path`, sehingga `"/tujuan.txt"` akan diarahkan menjadi file fisik seperti `/tmp/fuse_overlay_<PID>/tujuan.txt`. Setelah itu, file dibuka menggunakan `open()` dengan flag `O_CREAT | O_WRONLY | O_TRUNC`, yang berarti file akan dibuat jika belum ada, dibuka dalam mode tulis saja, dan dikosongkan jika sebelumnya sudah ada. Jika proses pembuatan gagal, fungsi mengembalikan `-errno` sebagai kode error FUSE. Jika berhasil, file descriptor disimpan ke `fi->fh` agar dapat digunakan oleh operasi FUSE berikutnya seperti write atau release. Pada dasarnya, fungsi ini membuat file kosong di overlay, dan khusus untuk `tujuan.txt`, keberadaan file kosong ini menjadi semacam “trigger” bahwa file virtual sudah aktif, sehingga setelah `touch` dijalankan, `tujuan_exists()` akan mengembalikan true dan `tujuan.txt` mulai bisa dibaca melalui konten yang dibangkitkan secara virtual.

#### HASIL AKHIR:
![alt text](image-7.png)

![alt text](image-8.png)
### d. Temukan Koordinat Ritual
Saat `cat mnt/tujuan.txt` dijalankan, isinya dibangkitkan on-the-fly (dibuat saat dibutuhkan, tidak disimpan di disk) dengan menggabungkan fragmen KOORD: <...> dari 1.txt sampai 7.txt. Format: "Tujuan Mas Amba: <gabungan fragmen>" diakhir tepat satu newline.

- Logika Utama
```c
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
```

#### 1 - Inisialisasi

```c
char result[8192];
int  rlen = snprintf(result, sizeof(result), "Tujuan Mas Amba: ");
```
Kode di atas berfugnsi untuk mengalokasikan buffer `result` untuk menyimpan string akhir denan inisialisasi prefix "Tujuan Mas Amba: ". Variabel `rlen` sendiri adalah pointer posisi untuk apend selanjutnya.

#### 2 - Loop Baca File

```c
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

```

Program lalu akan membuka dan membaca file `1.txt` hingga `7.txt` dan membaca baris per baris isi file tersebut. Program akan fokus mencari string "KOORD: " di setiap baris dan akan melewati baris yang tidak memiliki string yang dicari. Setelah menemukan string "KOORD: ", program akan melakukan parsing fragmen setelah string tersebutd dan melalukan append hasil fragmen ke `result`.

#### 4 - Newline

```c
if (rlen < (int)sizeof(result) - 1) {
    result[rlen++] = '\n';
    result[rlen]   = '\0';
}
```

Fungsi di atas akan menambah tepat satu newline di akhir dan set null terminator untuk string.

#### HASIL AKHIR:
![alt text](image-9.png)

### SOAL 2 -
![alt text](image-2.png)

![alt text](image-3.png)

![alt text](image-4.png)