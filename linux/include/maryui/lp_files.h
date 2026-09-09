/* The filesystem model behind the Finder, TextEdit and the Info window: a
 * directory listing with kinds, sizes and dates formatted the Finder's way,
 * path helpers, the user's folders and volumes, and the operations a file
 * manager needs (new folder, rename, trash, copy, move, delete). Everything
 * here is host-agnostic and free of UI: operations return 0 or -errno, and
 * nothing touches Cairo. Read directories in the EVENT pass, never in DRAW. */
#ifndef MARYUI_LP_FILES_H
#define MARYUI_LP_FILES_H

#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include "maryui/lp_icons.h"

#define LP_FILES_PATH_MAX 1024
#define LP_FILES_NAME_MAX 256

enum lp_file_kind { LP_FILE_FOLDER, LP_FILE_DOCUMENT, LP_FILE_IMAGE, LP_FILE_MUSIC, LP_FILE_CODE, LP_FILE_KIND_COUNT };
enum lp_file_sort { LP_FILE_SORT_NAME, LP_FILE_SORT_DATE, LP_FILE_SORT_SIZE, LP_FILE_SORT_KIND };

typedef struct lp_file_entry {
    char name[LP_FILES_NAME_MAX];
    enum lp_file_kind kind;
    int64_t size;             /* bytes; folders report their st_size but show "--" */
    time_t mtime;
    unsigned mode;            /* st_mode */
    unsigned uid;
    int is_dir, is_link, is_hidden;
} lp_file_entry;

typedef struct lp_file_list {
    lp_file_entry *entries;
    int count, cap;
    uint64_t dev;             /* the directory's st_dev (drops across devices copy) */
    uint64_t free_bytes;      /* statvfs at read time */
    time_t dir_mtime;
    int err;                  /* errno of a failed read, else 0 */
} lp_file_list;

/* Reads `dir` into `out` (which is reset first), sorted by name. 0 or -errno; a failed
 * read leaves an empty list with `err` set. Dotfiles are skipped unless show_hidden. */
int lp_files_list(const char *dir, int show_hidden, lp_file_list *out);
void lp_files_list_free(lp_file_list *l);
void lp_files_sort(lp_file_list *l, enum lp_file_sort by, int descending);
int lp_files_find(const lp_file_list *l, const char *name);
/* Finder order: case-insensitive, digit runs compared as numbers ("file2" < "file10"). */
int lp_files_compare_names(const char *a, const char *b);

enum lp_file_kind lp_files_kind(const char *name, int is_dir);
const char *lp_files_kind_label(enum lp_file_kind kind);   /* "Folder", "Document", … */
lp_icon lp_files_kind_icon(enum lp_file_kind kind);

/* "--" for folders; "512 bytes", "6 KB", "1.1 MB", "2.3 GB" (decimal units, like the Finder). */
void lp_files_format_size(int64_t bytes, int is_dir, char *out, size_t n);
/* "Today, 4:12 PM", "Yesterday, 3:48 PM", "Sep 5, 2026". */
void lp_files_format_date(time_t t, time_t now, char *out, size_t n);

/* Paths. */
void lp_files_join(const char *dir, const char *name, char *out, size_t n);
const char *lp_files_basename(const char *path);           /* "/" for the root */
void lp_files_parent(const char *path, char *out, size_t n); /* the root's parent is the root */
const char *lp_files_home(void);
void lp_files_abbreviate(const char *path, char *out, size_t n);   /* "~/Documents" */
/* The Finder's name for a folder: the user name for home, the OS name for "/", else the basename. */
void lp_files_display_name(const char *path, char *out, size_t n);
int lp_files_exists(const char *path, int *is_dir);
int lp_files_mkdir_p(const char *path);          /* 0 or -errno; an existing directory is fine */
int lp_files_same_device(const char *a, const char *b);
int lp_files_free_space(const char *path, uint64_t *out);
/* Plain text by extension, else by sniffing: no NUL and valid UTF-8 in the first 8 KB. */
int lp_files_is_text(const char *path);
/* Reads a whole file into a NUL-terminated malloc'd buffer (*len excludes the NUL). 0 or -errno. */
int lp_files_read(const char *path, char **out, size_t *len);
int lp_files_write(const char *path, const char *data, size_t len);

/* The user's folders and the Trash. */
enum lp_user_dir { LP_USER_HOME, LP_USER_DESKTOP, LP_USER_DOCUMENTS, LP_USER_DOWNLOADS, LP_USER_TRASH, LP_USER_DIR_COUNT };
void lp_files_user_dir(enum lp_user_dir which, char *out, size_t n);
const char *lp_files_user_dir_label(enum lp_user_dir which);
lp_icon lp_files_user_dir_icon(enum lp_user_dir which);
/* Creates ~/Desktop, ~/Documents and ~/Downloads when missing. 0 or the first -errno. */
int lp_files_user_dirs_ensure(void);

/* Volumes: "/" (named after the OS) first, then what is mounted under /media, /mnt and /run/media. */
typedef struct lp_volume { char name[64]; char path[LP_FILES_PATH_MAX]; } lp_volume;
int lp_files_volumes(lp_volume *out, int max);
int lp_files_volumes_parse(const char *mounts, const char *system_name, lp_volume *out, int max);

/* Operations: 0 or -errno. */
int lp_files_mkdir_unique(const char *dir, char *out_name, size_t n);    /* "untitled folder", "untitled folder 2" */
int lp_files_rename(const char *dir, const char *from, const char *to);  /* rejects "", "/", ".", "..", an existing name */
int lp_files_trash(const char *path);                                    /* freedesktop.org Trash, with a .trashinfo */
void lp_files_trash_dir(char *out, size_t n);                            /* $XDG_DATA_HOME/Trash (…/files holds the items) */
int lp_files_empty_trash(void);
/* Copies a file tree into dst_dir; a name clash (or the same directory) gets "x copy", "x copy 2", "x copy.txt". */
int lp_files_copy(const char *src, const char *dst_dir, char *out_name, size_t n);
/* Moves by rename, or copy + delete across devices. The name must be free in dst_dir. */
int lp_files_move(const char *src, const char *dst_dir);
int lp_files_delete_tree(const char *path);
/* The next free "name", "name 2", "name 3" in dir (extension-aware when keep_ext). */
void lp_files_unique_name(const char *dir, const char *name, const char *suffix, char *out, size_t n);

/* The process-wide file clipboard (Edit › Copy / Paste). */
typedef struct lp_file_clipboard { char (*paths)[LP_FILES_PATH_MAX]; int count; int cut; } lp_file_clipboard;
lp_file_clipboard *lp_files_clipboard_shared(void);
void lp_files_clipboard_set(lp_file_clipboard *c, const char *const *paths, int n, int cut);
void lp_files_clipboard_clear(lp_file_clipboard *c);

/* Tests: forces lp_files_move down the copy + delete path. */
extern int lp_files_force_move_fallback;

#endif
