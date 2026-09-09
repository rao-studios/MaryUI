/* lp_files.h: the filesystem behind the Finder. POSIX only; no UI. */
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>

#include "maryui/lp_files.h"
#include "maryui/lp_settings.h"

int lp_files_force_move_fallback = 0;

/* MARK: - Names and kinds */

int lp_files_compare_names(const char *a, const char *b) {
    const unsigned char *p = (const unsigned char *)a, *q = (const unsigned char *)b;
    while (*p && *q) {
        if (isdigit(*p) && isdigit(*q)) {
            while (*p == '0') p++;
            while (*q == '0') q++;
            const unsigned char *ps = p, *qs = q;
            while (isdigit(*p)) p++;
            while (isdigit(*q)) q++;
            long lp = p - ps, lq = q - qs;
            if (lp != lq) return lp < lq ? -1 : 1;
            int c = memcmp(ps, qs, (size_t)lp);
            if (c) return c;
            continue;
        }
        int ca = tolower(*p), cb = tolower(*q);
        if (ca != cb) return ca < cb ? -1 : 1;
        p++; q++;
    }
    if (*p || *q) return *p ? 1 : -1;
    return strcmp(a, b);
}

static const char *extension(const char *name) {
    const char *dot = strrchr(name, '.');
    if (!dot || dot == name || !dot[1]) return "";
    return dot + 1;
}

static int ext_in(const char *ext, const char *const *list) {
    for (int i = 0; list[i]; i++) if (strcasecmp(ext, list[i]) == 0) return 1;
    return 0;
}

static const char *const IMAGE_EXT[] = { "png", "jpg", "jpeg", "gif", "svg", "webp", "bmp", "tiff", "tif", "heic", "ico", "sketch", NULL };
static const char *const MUSIC_EXT[] = { "mp3", "m4a", "wav", "flac", "ogg", "aac", "aiff", "opus", NULL };
static const char *const CODE_EXT[] = { "c", "h", "cpp", "hpp", "cc", "ts", "tsx", "js", "jsx", "mjs", "cjs", "json", "sh", "py", "swift", "css",
    "conf", "toml", "yaml", "yml", "rs", "go", "java", "rb", "sql", "mk", "cmake", "ini", "cfg", "xml", "html", "htm", NULL };
static const char *const TEXT_EXT[] = { "txt", "md", "markdown", "text", "log", "csv", "tsv", "rtf", "tex", "nfo", "readme", NULL };

enum lp_file_kind lp_files_kind(const char *name, int is_dir) {
    if (is_dir) return LP_FILE_FOLDER;
    const char *ext = extension(name);
    if (ext_in(ext, IMAGE_EXT)) return LP_FILE_IMAGE;
    if (ext_in(ext, MUSIC_EXT)) return LP_FILE_MUSIC;
    if (ext_in(ext, CODE_EXT)) return LP_FILE_CODE;
    return LP_FILE_DOCUMENT;
}

const char *lp_files_kind_label(enum lp_file_kind kind) {
    static const char *const LABELS[] = { "Folder", "Document", "Image", "Audio", "Source" };
    return kind < LP_FILE_KIND_COUNT ? LABELS[kind] : "Document";
}

lp_icon lp_files_kind_icon(enum lp_file_kind kind) {
    static const lp_icon ICONS[] = { LP_ICON_FOLDER, LP_ICON_DOCUMENT, LP_ICON_IMAGE, LP_ICON_MUSIC, LP_ICON_CODE };
    return kind < LP_FILE_KIND_COUNT ? ICONS[kind] : LP_ICON_DOCUMENT;
}

/* MARK: - Formatting */

void lp_files_format_size(int64_t bytes, int is_dir, char *out, size_t n) {
    if (is_dir) { snprintf(out, n, "--"); return; }
    if (bytes < 1000) { snprintf(out, n, "%lld byte%s", (long long)bytes, bytes == 1 ? "" : "s"); return; }
    double v = (double)bytes;
    if (v < 1e6) { snprintf(out, n, "%.0f KB", v / 1e3); return; }
    if (v < 1e9) { snprintf(out, n, "%.1f MB", v / 1e6); return; }
    if (v < 1e12) { snprintf(out, n, "%.1f GB", v / 1e9); return; }
    snprintf(out, n, "%.2f TB", v / 1e12);
}

void lp_files_format_date(time_t t, time_t now, char *out, size_t n) {
    struct tm tm, tn;
    localtime_r(&t, &tm);
    localtime_r(&now, &tn);
    int hour = tm.tm_hour % 12;
    if (hour == 0) hour = 12;
    const char *ampm = tm.tm_hour < 12 ? "AM" : "PM";
    if (tm.tm_year == tn.tm_year && tm.tm_yday == tn.tm_yday) { snprintf(out, n, "Today, %d:%02d %s", hour, tm.tm_min, ampm); return; }
    time_t yesterday = now - 86400;
    struct tm ty;
    localtime_r(&yesterday, &ty);
    if (tm.tm_year == ty.tm_year && tm.tm_yday == ty.tm_yday) { snprintf(out, n, "Yesterday, %d:%02d %s", hour, tm.tm_min, ampm); return; }
    char month[8];
    strftime(month, sizeof month, "%b", &tm);
    snprintf(out, n, "%s %d, %d", month, tm.tm_mday, tm.tm_year + 1900);
}

/* MARK: - Paths */

void lp_files_join(const char *dir, const char *name, char *out, size_t n) {
    size_t len = strlen(dir);
    if (len > 0 && dir[len - 1] == '/') snprintf(out, n, "%s%s", dir, name);
    else snprintf(out, n, "%s/%s", dir, name);
}

const char *lp_files_basename(const char *path) {
    const char *slash = strrchr(path, '/');
    if (!slash) return path;
    if (!slash[1]) return path[1] ? path : "/"; /* "a/b/" is unusual; the root is "/" */
    return slash + 1;
}

void lp_files_parent(const char *path, char *out, size_t n) {
    snprintf(out, n, "%s", path);
    size_t len = strlen(out);
    while (len > 1 && out[len - 1] == '/') out[--len] = 0;
    char *slash = strrchr(out, '/');
    if (!slash) { snprintf(out, n, "/"); return; }
    if (slash == out) out[1] = 0; else *slash = 0;
}

const char *lp_files_home(void) {
    const char *h = getenv("HOME");
    return h && *h ? h : "/tmp";
}

void lp_files_abbreviate(const char *path, char *out, size_t n) {
    const char *home = lp_files_home();
    size_t hl = strlen(home);
    if (strncmp(path, home, hl) == 0 && (path[hl] == 0 || path[hl] == '/')) snprintf(out, n, "~%s", path + hl);
    else snprintf(out, n, "%s", path);
}

void lp_files_display_name(const char *path, char *out, size_t n) {
    if (strcmp(path, "/") == 0) {
        lp_branding b = lp_branding_load();
        snprintf(out, n, "%s", b.name[0] ? b.name : "System");
        return;
    }
    char trash[LP_FILES_PATH_MAX];
    lp_files_user_dir(LP_USER_TRASH, trash, sizeof trash);
    if (strcmp(path, trash) == 0) { snprintf(out, n, "Trash"); return; }
    snprintf(out, n, "%s", lp_files_basename(path));
}

int lp_files_exists(const char *path, int *is_dir) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    if (is_dir) *is_dir = S_ISDIR(st.st_mode);
    return 1;
}

int lp_files_same_device(const char *a, const char *b) {
    struct stat sa, sb;
    if (stat(a, &sa) != 0 || stat(b, &sb) != 0) return 1;
    return sa.st_dev == sb.st_dev;
}

int lp_files_free_space(const char *path, uint64_t *out) {
    struct statvfs vfs;
    if (statvfs(path, &vfs) != 0) return -errno;
    *out = (uint64_t)vfs.f_bavail * (uint64_t)vfs.f_frsize;
    return 0;
}

static int valid_utf8(const unsigned char *s, size_t n) {
    size_t i = 0;
    while (i < n) {
        unsigned char c = s[i];
        size_t len = c < 0x80 ? 1 : (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : (c & 0xF8) == 0xF0 ? 4 : 0;
        if (len == 0) return 0;
        if (i + len > n) return 1; /* a sequence cut by the sniff window */
        for (size_t k = 1; k < len; k++) if ((s[i + k] & 0xC0) != 0x80) return 0;
        i += len;
    }
    return 1;
}

int lp_files_is_text(const char *path) {
    const char *ext = extension(lp_files_basename(path));
    if (ext_in(ext, TEXT_EXT) || ext_in(ext, CODE_EXT)) return 1;
    if (ext_in(ext, IMAGE_EXT) || ext_in(ext, MUSIC_EXT)) return 0;
    int fd = open(path, O_RDONLY);
    if (fd < 0) return 0;
    unsigned char buf[8192];
    ssize_t n = read(fd, buf, sizeof buf);
    close(fd);
    if (n < 0) return 0;
    if (n == 0) return 1;
    if (memchr(buf, 0, (size_t)n)) return 0;
    return valid_utf8(buf, (size_t)n);
}

int lp_files_read(const char *path, char **out, size_t *len) {
    FILE *f = fopen(path, "rb");
    if (!f) return -errno;
    char *buf = NULL;
    size_t cap = 0, n = 0;
    char chunk[4096];
    size_t got;
    while ((got = fread(chunk, 1, sizeof chunk, f)) > 0) {
        if (n + got + 1 > cap) {
            cap = (n + got + 1) * 2;
            char *grown = realloc(buf, cap);
            if (!grown) { free(buf); fclose(f); return -ENOMEM; }
            buf = grown;
        }
        memcpy(buf + n, chunk, got);
        n += got;
    }
    fclose(f);
    if (!buf) { buf = malloc(1); if (!buf) return -ENOMEM; }
    buf[n] = 0;
    *out = buf;
    if (len) *len = n;
    return 0;
}

int lp_files_write(const char *path, const char *data, size_t len) {
    FILE *f = fopen(path, "wb");
    if (!f) return -errno;
    int rc = 0;
    if (len && fwrite(data, 1, len, f) != len) rc = -(errno ? errno : EIO);
    if (fclose(f) != 0 && rc == 0) rc = -errno;
    return rc;
}

/* MARK: - Listing */

void lp_files_list_free(lp_file_list *l) {
    free(l->entries);
    memset(l, 0, sizeof *l);
}

static int push(lp_file_list *l, const lp_file_entry *e) {
    if (l->count == l->cap) {
        int cap = l->cap ? l->cap * 2 : 64;
        lp_file_entry *grown = realloc(l->entries, (size_t)cap * sizeof *grown);
        if (!grown) return -ENOMEM;
        l->entries = grown;
        l->cap = cap;
    }
    l->entries[l->count++] = *e;
    return 0;
}

static int cmp_name(const void *a, const void *b) { return lp_files_compare_names(((const lp_file_entry *)a)->name, ((const lp_file_entry *)b)->name); }

int lp_files_list(const char *dir, int show_hidden, lp_file_list *out) {
    lp_files_list_free(out);
    struct stat dst;
    if (stat(dir, &dst) != 0) { out->err = errno; return -errno; }
    out->dev = (uint64_t)dst.st_dev;
    out->dir_mtime = dst.st_mtime;
    lp_files_free_space(dir, &out->free_bytes);
    DIR *d = opendir(dir);
    if (!d) { out->err = errno; return -errno; }
    struct dirent *de;
    char path[LP_FILES_PATH_MAX];
    while ((de = readdir(d)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
        int hidden = de->d_name[0] == '.';
        if (hidden && !show_hidden) continue;
        lp_file_entry e;
        memset(&e, 0, sizeof e);
        snprintf(e.name, sizeof e.name, "%s", de->d_name);
        e.is_hidden = hidden;
        lp_files_join(dir, de->d_name, path, sizeof path);
        struct stat st;
        if (lstat(path, &st) != 0) continue;
        if (S_ISLNK(st.st_mode)) {
            e.is_link = 1;
            struct stat target;
            if (stat(path, &target) == 0) st = target;
        }
        e.is_dir = S_ISDIR(st.st_mode);
        e.size = (int64_t)st.st_size;
        e.mtime = st.st_mtime;
        e.mode = (unsigned)st.st_mode;
        e.uid = (unsigned)st.st_uid;
        e.kind = lp_files_kind(e.name, e.is_dir);
        int rc = push(out, &e);
        if (rc) { closedir(d); return rc; }
    }
    closedir(d);
    if (out->count > 1) qsort(out->entries, (size_t)out->count, sizeof out->entries[0], cmp_name);
    return 0;
}

static enum lp_file_sort sort_by;
static int sort_desc;
static int cmp_sorted(const void *pa, const void *pb) {
    const lp_file_entry *a = pa, *b = pb;
    int c = 0;
    switch (sort_by) {
    case LP_FILE_SORT_DATE: c = a->mtime < b->mtime ? -1 : a->mtime > b->mtime ? 1 : 0; break;
    case LP_FILE_SORT_SIZE: {
        int64_t sa = a->is_dir ? -1 : a->size, sb = b->is_dir ? -1 : b->size;
        c = sa < sb ? -1 : sa > sb ? 1 : 0;
        break;
    }
    case LP_FILE_SORT_KIND: c = (int)a->kind - (int)b->kind; break;
    default: break;
    }
    if (sort_desc) c = -c;
    if (c == 0) c = lp_files_compare_names(a->name, b->name);
    return c;
}

void lp_files_sort(lp_file_list *l, enum lp_file_sort by, int descending) {
    sort_by = by;
    sort_desc = descending;
    if (l->count > 1) qsort(l->entries, (size_t)l->count, sizeof l->entries[0], cmp_sorted);
}

int lp_files_find(const lp_file_list *l, const char *name) {
    for (int i = 0; i < l->count; i++) if (strcmp(l->entries[i].name, name) == 0) return i;
    return -1;
}

/* MARK: - User folders, Trash, volumes */

void lp_files_trash_dir(char *out, size_t n) {
    const char *xdg = getenv("XDG_DATA_HOME");
    if (xdg && *xdg) snprintf(out, n, "%s/Trash", xdg);
    else snprintf(out, n, "%s/.local/share/Trash", lp_files_home());
}

void lp_files_user_dir(enum lp_user_dir which, char *out, size_t n) {
    const char *home = lp_files_home();
    switch (which) {
    case LP_USER_DESKTOP: snprintf(out, n, "%s/Desktop", home); break;
    case LP_USER_DOCUMENTS: snprintf(out, n, "%s/Documents", home); break;
    case LP_USER_DOWNLOADS: snprintf(out, n, "%s/Downloads", home); break;
    case LP_USER_TRASH: { char t[LP_FILES_PATH_MAX]; lp_files_trash_dir(t, sizeof t); snprintf(out, n, "%s/files", t); break; }
    default: snprintf(out, n, "%s", home); break;
    }
}

const char *lp_files_user_dir_label(enum lp_user_dir which) {
    switch (which) {
    case LP_USER_DESKTOP: return "Desktop";
    case LP_USER_DOCUMENTS: return "Documents";
    case LP_USER_DOWNLOADS: return "Downloads";
    case LP_USER_TRASH: return "Trash";
    default: {
        static char name[64];
        struct passwd *pw = getpwuid(getuid());
        snprintf(name, sizeof name, "%s", pw && pw->pw_name ? pw->pw_name : lp_files_basename(lp_files_home()));
        return name;
    }
    }
}

lp_icon lp_files_user_dir_icon(enum lp_user_dir which) {
    switch (which) {
    case LP_USER_DESKTOP: return LP_ICON_DESKTOP;
    case LP_USER_DOCUMENTS: return LP_ICON_DOCUMENT;
    case LP_USER_DOWNLOADS: return LP_ICON_DOWNLOAD;
    case LP_USER_TRASH: return LP_ICON_TRASH;
    default: return LP_ICON_HOME;
    }
}

static int mkdir_p(const char *path, mode_t mode) {
    char tmp[LP_FILES_PATH_MAX];
    snprintf(tmp, sizeof tmp, "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p != '/') continue;
        *p = 0;
        if (mkdir(tmp, mode) != 0 && errno != EEXIST) return -errno;
        *p = '/';
    }
    if (mkdir(tmp, mode) != 0 && errno != EEXIST) return -errno;
    return 0;
}

int lp_files_mkdir_p(const char *path) { return mkdir_p(path, 0755); }

int lp_files_user_dirs_ensure(void) {
    int first = 0;
    for (enum lp_user_dir k = LP_USER_DESKTOP; k <= LP_USER_DOWNLOADS; k++) {
        char path[LP_FILES_PATH_MAX];
        lp_files_user_dir(k, path, sizeof path);
        int rc = mkdir_p(path, 0755);
        if (rc && !first) first = rc;
    }
    return first;
}

static void unescape_mount(const char *in, char *out, size_t n) {
    size_t o = 0;
    for (const char *p = in; *p && o + 1 < n; p++) {
        if (*p == '\\' && isdigit((unsigned char)p[1]) && isdigit((unsigned char)p[2]) && isdigit((unsigned char)p[3])) {
            out[o++] = (char)((p[1] - '0') * 64 + (p[2] - '0') * 8 + (p[3] - '0'));
            p += 3;
        } else {
            out[o++] = *p;
        }
    }
    out[o] = 0;
}

static int virtual_fs(const char *type) {
    static const char *const VIRTUAL[] = { "proc", "sysfs", "devtmpfs", "devpts", "tmpfs", "cgroup", "cgroup2", "securityfs", "pstore", "debugfs",
        "configfs", "fusectl", "mqueue", "hugetlbfs", "tracefs", "binfmt_misc", "autofs", "nsfs", "bpf", "efivarfs", "ramfs", "rpc_pipefs", "squashfs", NULL };
    return ext_in(type, VIRTUAL);
}

int lp_files_volumes_parse(const char *mounts, const char *system_name, lp_volume *out, int max) {
    int n = 0;
    if (max <= 0) return 0;
    snprintf(out[0].name, sizeof out[0].name, "%s", system_name && *system_name ? system_name : "System");
    snprintf(out[0].path, sizeof out[0].path, "/");
    n = 1;
    const char *line = mounts;
    while (line && *line && n < max) {
        const char *end = strchr(line, '\n');
        size_t len = end ? (size_t)(end - line) : strlen(line);
        char buf[2048];
        if (len >= sizeof buf) len = sizeof buf - 1;
        memcpy(buf, line, len);
        buf[len] = 0;
        line = end ? end + 1 : NULL;
        char dev[512], mnt[1024], type[64];
        if (sscanf(buf, "%511s %1023s %63s", dev, mnt, type) != 3) continue;
        if (virtual_fs(type)) continue;
        char path[LP_FILES_PATH_MAX];
        unescape_mount(mnt, path, sizeof path);
        if (strncmp(path, "/media/", 7) != 0 && strncmp(path, "/mnt/", 5) != 0 && strncmp(path, "/run/media/", 11) != 0) continue;
        int dup = 0;
        for (int i = 0; i < n; i++) if (strcmp(out[i].path, path) == 0) dup = 1;
        if (dup) continue;
        snprintf(out[n].path, sizeof out[n].path, "%s", path);
        snprintf(out[n].name, sizeof out[n].name, "%s", lp_files_basename(path));
        n++;
    }
    return n;
}

int lp_files_volumes(lp_volume *out, int max) {
    lp_branding b = lp_branding_load();
    char *text = NULL;
    size_t len = 0;
    if (lp_files_read("/proc/mounts", &text, &len) != 0) text = NULL;
    int n = lp_files_volumes_parse(text ? text : "", b.name, out, max);
    free(text);
    return n;
}

/* MARK: - Operations */

void lp_files_unique_name(const char *dir, const char *name, const char *suffix, char *out, size_t n) {
    /* "name", "name<suffix>", "name<suffix> 2", …; the extension (if any) stays at the end */
    char stem[LP_FILES_NAME_MAX], ext[LP_FILES_NAME_MAX];
    const char *dot = strrchr(name, '.');
    if (dot && dot != name && dot[1]) { snprintf(stem, sizeof stem, "%.*s", (int)(dot - name), name); snprintf(ext, sizeof ext, "%s", dot); }
    else { snprintf(stem, sizeof stem, "%s", name); ext[0] = 0; }
    char path[LP_FILES_PATH_MAX];
    for (int i = 1; i < 10000; i++) {
        if (i == 1 && !(suffix && *suffix)) snprintf(out, n, "%s%s", stem, ext);
        else if (i == 1) snprintf(out, n, "%s%s%s", stem, suffix, ext);
        else snprintf(out, n, "%s%s %d%s", stem, suffix ? suffix : "", i, ext);
        lp_files_join(dir, out, path, sizeof path);
        struct stat st;
        if (lstat(path, &st) != 0) return;
    }
}

int lp_files_mkdir_unique(const char *dir, char *out_name, size_t n) {
    lp_files_unique_name(dir, "untitled folder", "", out_name, n);
    char path[LP_FILES_PATH_MAX];
    lp_files_join(dir, out_name, path, sizeof path);
    if (mkdir(path, 0755) != 0) return -errno;
    return 0;
}

static int valid_name(const char *name) {
    if (!name || !*name) return 0;
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) return 0;
    if (strchr(name, '/')) return 0;
    if (strlen(name) >= LP_FILES_NAME_MAX) return 0;
    return 1;
}

int lp_files_rename(const char *dir, const char *from, const char *to) {
    if (!valid_name(from) || !valid_name(to)) return -EINVAL;
    if (strcmp(from, to) == 0) return 0;
    char a[LP_FILES_PATH_MAX], b[LP_FILES_PATH_MAX];
    lp_files_join(dir, from, a, sizeof a);
    lp_files_join(dir, to, b, sizeof b);
    struct stat st;
    if (lstat(b, &st) == 0) return -EEXIST;
    if (rename(a, b) != 0) return -errno;
    return 0;
}

static int copy_file(const char *src, const char *dst, mode_t mode) {
    int in = open(src, O_RDONLY);
    if (in < 0) return -errno;
    int out = open(dst, O_WRONLY | O_CREAT | O_EXCL, mode & 0777);
    if (out < 0) { int e = errno; close(in); return -e; }
    char buf[65536];
    ssize_t n;
    int rc = 0;
    while ((n = read(in, buf, sizeof buf)) > 0) {
        ssize_t off = 0;
        while (off < n) {
            ssize_t w = write(out, buf + off, (size_t)(n - off));
            if (w < 0) { rc = -errno; break; }
            off += w;
        }
        if (rc) break;
    }
    if (n < 0 && !rc) rc = -errno;
    close(in);
    if (close(out) != 0 && !rc) rc = -errno;
    return rc;
}

static int copy_tree(const char *src, const char *dst) {
    struct stat st;
    if (lstat(src, &st) != 0) return -errno;
    if (S_ISLNK(st.st_mode)) {
        char target[LP_FILES_PATH_MAX];
        ssize_t n = readlink(src, target, sizeof target - 1);
        if (n < 0) return -errno;
        target[n] = 0;
        if (symlink(target, dst) != 0) return -errno;
        return 0;
    }
    if (S_ISDIR(st.st_mode)) {
        if (mkdir(dst, st.st_mode & 0777) != 0) return -errno;
        DIR *d = opendir(src);
        if (!d) return -errno;
        struct dirent *de;
        int rc = 0;
        while (!rc && (de = readdir(d)) != NULL) {
            if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
            char a[LP_FILES_PATH_MAX], b[LP_FILES_PATH_MAX];
            lp_files_join(src, de->d_name, a, sizeof a);
            lp_files_join(dst, de->d_name, b, sizeof b);
            rc = copy_tree(a, b);
        }
        closedir(d);
        return rc;
    }
    return copy_file(src, dst, st.st_mode);
}

int lp_files_copy(const char *src, const char *dst_dir, char *out_name, size_t n) {
    const char *name = lp_files_basename(src);
    if (!valid_name(name)) return -EINVAL;
    struct stat st;
    if (lstat(src, &st) != 0) return -errno;
    char parent[LP_FILES_PATH_MAX], probe[LP_FILES_PATH_MAX], chosen[LP_FILES_NAME_MAX];
    lp_files_parent(src, parent, sizeof parent);
    lp_files_join(dst_dir, name, probe, sizeof probe);
    struct stat clash;
    int same_dir = strcmp(parent, dst_dir) == 0;
    if (same_dir || lstat(probe, &clash) == 0) lp_files_unique_name(dst_dir, name, " copy", chosen, sizeof chosen);
    else snprintf(chosen, sizeof chosen, "%s", name);
    char dst[LP_FILES_PATH_MAX];
    lp_files_join(dst_dir, chosen, dst, sizeof dst);
    /* Never copy a folder into itself. */
    size_t sl = strlen(src);
    if (S_ISDIR(st.st_mode) && strncmp(dst, src, sl) == 0 && (dst[sl] == '/' || dst[sl] == 0)) return -EINVAL;
    int rc = copy_tree(src, dst);
    if (rc) { lp_files_delete_tree(dst); return rc; }
    if (out_name) snprintf(out_name, n, "%s", chosen);
    return 0;
}

int lp_files_delete_tree(const char *path) {
    struct stat st;
    if (lstat(path, &st) != 0) return -errno;
    if (S_ISDIR(st.st_mode)) {
        DIR *d = opendir(path);
        if (!d) return -errno;
        struct dirent *de;
        int rc = 0;
        while (!rc && (de = readdir(d)) != NULL) {
            if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
            char child[LP_FILES_PATH_MAX];
            lp_files_join(path, de->d_name, child, sizeof child);
            rc = lp_files_delete_tree(child);
        }
        closedir(d);
        if (rc) return rc;
        if (rmdir(path) != 0) return -errno;
        return 0;
    }
    if (unlink(path) != 0) return -errno;
    return 0;
}

int lp_files_move(const char *src, const char *dst_dir) {
    const char *name = lp_files_basename(src);
    if (!valid_name(name)) return -EINVAL;
    char dst[LP_FILES_PATH_MAX];
    lp_files_join(dst_dir, name, dst, sizeof dst);
    if (strcmp(src, dst) == 0) return 0;
    struct stat st;
    if (lstat(dst, &st) == 0) return -EEXIST;
    if (lstat(src, &st) != 0) return -errno;
    size_t sl = strlen(src);
    if (S_ISDIR(st.st_mode) && strncmp(dst_dir, src, sl) == 0 && (dst_dir[sl] == '/' || dst_dir[sl] == 0)) return -EINVAL;
    if (!lp_files_force_move_fallback) {
        if (rename(src, dst) == 0) return 0;
        if (errno != EXDEV) return -errno;
    }
    int rc = copy_tree(src, dst);
    if (rc) { lp_files_delete_tree(dst); return rc; }
    return lp_files_delete_tree(src);
}

static void percent_encode(const char *in, char *out, size_t n) {
    static const char HEX[] = "0123456789ABCDEF";
    size_t o = 0;
    for (const unsigned char *p = (const unsigned char *)in; *p && o + 4 < n; p++) {
        if (isalnum(*p) || strchr("/-._~", *p)) out[o++] = (char)*p;
        else { out[o++] = '%'; out[o++] = HEX[*p >> 4]; out[o++] = HEX[*p & 15]; }
    }
    out[o] = 0;
}

int lp_files_trash(const char *path) {
    const char *name = lp_files_basename(path);
    if (!valid_name(name)) return -EINVAL;
    struct stat st;
    if (lstat(path, &st) != 0) return -errno;
    char trash[LP_FILES_PATH_MAX], files[LP_FILES_PATH_MAX], info[LP_FILES_PATH_MAX];
    lp_files_trash_dir(trash, sizeof trash);
    snprintf(files, sizeof files, "%s/files", trash);
    snprintf(info, sizeof info, "%s/info", trash);
    int rc = mkdir_p(files, 0700);
    if (!rc) rc = mkdir_p(info, 0700);
    if (rc) return rc;
    char chosen[LP_FILES_NAME_MAX];
    lp_files_unique_name(files, name, "", chosen, sizeof chosen);
    /* the .trashinfo must be free too */
    char infopath[LP_FILES_PATH_MAX];
    for (int i = 2; i < 10000; i++) {
        snprintf(infopath, sizeof infopath, "%s/%s.trashinfo", info, chosen);
        struct stat s;
        if (lstat(infopath, &s) != 0) break;
        snprintf(chosen, sizeof chosen, "%s %d", name, i);
    }
    char encoded[LP_FILES_PATH_MAX * 3];
    percent_encode(path, encoded, sizeof encoded);
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    char stamp[32];
    strftime(stamp, sizeof stamp, "%Y-%m-%dT%H:%M:%S", &tm);
    FILE *f = fopen(infopath, "wx");
    if (!f) return -errno;
    fprintf(f, "[Trash Info]\nPath=%s\nDeletionDate=%s\n", encoded, stamp);
    fclose(f);
    char dst[LP_FILES_PATH_MAX];
    lp_files_join(files, chosen, dst, sizeof dst);
    if (!lp_files_force_move_fallback && rename(path, dst) == 0) return 0;
    if (!lp_files_force_move_fallback && errno != EXDEV) { int e = errno; unlink(infopath); return -e; }
    rc = copy_tree(path, dst);
    if (rc) { lp_files_delete_tree(dst); unlink(infopath); return rc; }
    return lp_files_delete_tree(path);
}

int lp_files_empty_trash(void) {
    char trash[LP_FILES_PATH_MAX];
    lp_files_trash_dir(trash, sizeof trash);
    static const char *const SUB[] = { "files", "info" };
    int first = 0;
    for (int s = 0; s < 2; s++) {
        char dir[LP_FILES_PATH_MAX];
        snprintf(dir, sizeof dir, "%s/%s", trash, SUB[s]);
        DIR *d = opendir(dir);
        if (!d) continue;
        struct dirent *de;
        while ((de = readdir(d)) != NULL) {
            if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
            char child[LP_FILES_PATH_MAX];
            lp_files_join(dir, de->d_name, child, sizeof child);
            int rc = lp_files_delete_tree(child);
            if (rc && !first) first = rc;
        }
        closedir(d);
    }
    return first;
}

/* MARK: - Clipboard */

lp_file_clipboard *lp_files_clipboard_shared(void) {
    static lp_file_clipboard shared;
    return &shared;
}

void lp_files_clipboard_clear(lp_file_clipboard *c) {
    free(c->paths);
    c->paths = NULL;
    c->count = 0;
    c->cut = 0;
}

void lp_files_clipboard_set(lp_file_clipboard *c, const char *const *paths, int n, int cut) {
    lp_files_clipboard_clear(c);
    if (n <= 0) return;
    c->paths = calloc((size_t)n, sizeof *c->paths);
    if (!c->paths) return;
    for (int i = 0; i < n; i++) snprintf(c->paths[i], sizeof c->paths[i], "%s", paths[i]);
    c->count = n;
    c->cut = cut;
}
