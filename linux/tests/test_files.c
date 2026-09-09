/* lp_files: the Finder's filesystem model, against a throwaway tree. */
#define _DARWIN_C_SOURCE 1
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "lp_test.h"
#include "maryui/lp_files.h"

static char root[512];

static void write_bytes(const char *rel, const char *data, size_t n) {
    char path[1024];
    snprintf(path, sizeof path, "%s/%s", root, rel);
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); exit(1); }
    fwrite(data, 1, n, f);
    fclose(f);
}
static void write_file(const char *rel, const char *text) { write_bytes(rel, text, strlen(text)); }
static void make_dir(const char *rel) {
    char path[1024];
    snprintf(path, sizeof path, "%s/%s", root, rel);
    mkdir(path, 0755);
}
static int exists(const char *rel) {
    char path[1024];
    snprintf(path, sizeof path, "%s/%s", root, rel);
    struct stat st;
    return lstat(path, &st) == 0;
}
static char *abs_path(const char *rel) {
    static char buf[8][1024];
    static int i;
    char *p = buf[i++ & 7];
    snprintf(p, 1024, "%s/%s", root, rel);
    return p;
}

static void setup(void) {
    const char *tmp = getenv("TMPDIR");
    snprintf(root, sizeof root, "%s/lp_files_XXXXXX", tmp && *tmp ? tmp : "/tmp");
    if (!mkdtemp(root)) { perror("mkdtemp"); exit(1); }
    setenv("HOME", root, 1);
    char xdg[600];
    snprintf(xdg, sizeof xdg, "%s/.local/share", root);
    setenv("XDG_DATA_HOME", xdg, 1);
    make_dir("Documents");
    write_file("Documents/beta.txt", "hello");
    write_file("Documents/alpha.md", "# a\n");
    write_file("Documents/file10.txt", "");
    write_file("Documents/file2.txt", "");
    write_file("Documents/.hidden", "");
    make_dir("Documents/Notes");
    write_file("Documents/Notes/inner.txt", "inner");
    write_file("Documents/photo.png", "\x89PNG\0\0binary");
    write_file("Documents/song.m4a", "");
    write_file("Documents/main.c", "int main(void){}");
}

static void teardown(void) { lp_files_delete_tree(root); }

LP_TEST(lists_a_directory_sorted_naturally_without_dotfiles) {
    lp_file_list l = { 0 };
    LP_ASSERT_EQ(lp_files_list(abs_path("Documents"), 0, &l), 0);
    LP_ASSERT_EQ(l.count, 8);
    LP_ASSERT_STR(l.entries[0].name, "alpha.md");
    LP_ASSERT_STR(l.entries[1].name, "beta.txt");
    LP_ASSERT_STR(l.entries[2].name, "file2.txt");
    LP_ASSERT_STR(l.entries[3].name, "file10.txt");
    LP_ASSERT_STR(l.entries[4].name, "main.c");
    LP_ASSERT_STR(l.entries[5].name, "Notes");
    LP_ASSERT(l.entries[5].is_dir);
    LP_ASSERT_EQ(l.entries[5].kind, LP_FILE_FOLDER);
    LP_ASSERT_EQ(l.entries[6].kind, LP_FILE_IMAGE);
    LP_ASSERT_EQ(l.entries[7].kind, LP_FILE_MUSIC);
    LP_ASSERT_EQ(l.entries[4].kind, LP_FILE_CODE);
    LP_ASSERT_EQ(l.entries[1].size, 5);
    LP_ASSERT(l.free_bytes > 0);
    LP_ASSERT_EQ(lp_files_find(&l, "Notes"), 5);
    LP_ASSERT_EQ(lp_files_find(&l, "nope"), -1);
    LP_ASSERT_EQ(lp_files_list(abs_path("Documents"), 1, &l), 0);
    LP_ASSERT_EQ(l.count, 9);
    LP_ASSERT_STR(l.entries[0].name, ".hidden");
    LP_ASSERT(l.entries[0].is_hidden);
    LP_ASSERT_EQ(lp_files_list(abs_path("missing"), 0, &l), -ENOENT);
    LP_ASSERT_EQ(l.count, 0);
    LP_ASSERT_EQ(l.err, ENOENT);
    lp_files_list_free(&l);
}

LP_TEST(sorts_by_column_with_names_as_the_tie_breaker) {
    lp_file_list l = { 0 };
    lp_files_list(abs_path("Documents"), 0, &l);
    lp_files_sort(&l, LP_FILE_SORT_KIND, 0);
    LP_ASSERT_STR(l.entries[0].name, "Notes");
    LP_ASSERT_STR(l.entries[1].name, "alpha.md");
    lp_files_sort(&l, LP_FILE_SORT_SIZE, 1);
    LP_ASSERT_STR(l.entries[0].name, "main.c");
    LP_ASSERT_STR(l.entries[l.count - 1].name, "Notes");
    lp_files_sort(&l, LP_FILE_SORT_NAME, 0);
    LP_ASSERT_STR(l.entries[0].name, "alpha.md");
    lp_files_list_free(&l);
}

LP_TEST(compares_names_like_the_finder) {
    LP_ASSERT(lp_files_compare_names("apple", "Banana") < 0);
    LP_ASSERT(lp_files_compare_names("file2", "file10") < 0);
    LP_ASSERT(lp_files_compare_names("File10", "file9") > 0);
    LP_ASSERT_EQ(lp_files_compare_names("same", "same"), 0);
    LP_ASSERT(lp_files_compare_names("a", "a b") < 0);
}

LP_TEST(formats_sizes_and_dates) {
    char s[32];
    lp_files_format_size(0, 1, s, sizeof s); LP_ASSERT_STR(s, "--");
    lp_files_format_size(1, 0, s, sizeof s); LP_ASSERT_STR(s, "1 byte");
    lp_files_format_size(512, 0, s, sizeof s); LP_ASSERT_STR(s, "512 bytes");
    lp_files_format_size(6144, 0, s, sizeof s); LP_ASSERT_STR(s, "6 KB");
    lp_files_format_size(210000, 0, s, sizeof s); LP_ASSERT_STR(s, "210 KB");
    lp_files_format_size(1100000, 0, s, sizeof s); LP_ASSERT_STR(s, "1.1 MB");
    lp_files_format_size(18400000, 0, s, sizeof s); LP_ASSERT_STR(s, "18.4 MB");
    lp_files_format_size(2300000000LL, 0, s, sizeof s); LP_ASSERT_STR(s, "2.3 GB");
    setenv("TZ", "UTC", 1);
    tzset();
    struct tm at = { 0 };
    at.tm_year = 2026 - 1900; at.tm_mon = 8; at.tm_mday = 8; at.tm_hour = 20; at.tm_min = 32;
    time_t now = mktime(&at); /* 2026-09-08 20:32:00 UTC */
    lp_files_format_date(now - 3600, now, s, sizeof s); LP_ASSERT_STR(s, "Today, 7:32 PM");
    lp_files_format_date(now - 86400, now, s, sizeof s); LP_ASSERT_STR(s, "Yesterday, 8:32 PM");
    lp_files_format_date(now - 3 * 86400, now, s, sizeof s); LP_ASSERT_STR(s, "Sep 5, 2026");
    lp_files_format_date(0, now, s, sizeof s); LP_ASSERT_STR(s, "Jan 1, 1970");
}

LP_TEST(path_helpers) {
    char out[1024];
    lp_files_join("/a/b", "c", out, sizeof out); LP_ASSERT_STR(out, "/a/b/c");
    lp_files_join("/", "c", out, sizeof out); LP_ASSERT_STR(out, "/c");
    LP_ASSERT_STR(lp_files_basename("/a/b/c.txt"), "c.txt");
    LP_ASSERT_STR(lp_files_basename("/"), "/");
    lp_files_parent("/a/b/c", out, sizeof out); LP_ASSERT_STR(out, "/a/b");
    lp_files_parent("/a", out, sizeof out); LP_ASSERT_STR(out, "/");
    lp_files_parent("/", out, sizeof out); LP_ASSERT_STR(out, "/");
    lp_files_abbreviate(abs_path("Documents"), out, sizeof out); LP_ASSERT_STR(out, "~/Documents");
    lp_files_abbreviate("/etc", out, sizeof out); LP_ASSERT_STR(out, "/etc");
    lp_files_display_name(abs_path("Documents"), out, sizeof out); LP_ASSERT_STR(out, "Documents");
    lp_files_display_name("/", out, sizeof out); LP_ASSERT(out[0] != 0 && strcmp(out, "/") != 0);
    char trash[1024];
    lp_files_user_dir(LP_USER_TRASH, trash, sizeof trash);
    lp_files_display_name(trash, out, sizeof out); LP_ASSERT_STR(out, "Trash");
    lp_files_user_dir(LP_USER_DOCUMENTS, out, sizeof out); LP_ASSERT_STR(out, abs_path("Documents"));
    LP_ASSERT_STR(lp_files_user_dir_label(LP_USER_DOWNLOADS), "Downloads");
    int is_dir = 0;
    LP_ASSERT(lp_files_exists(abs_path("Documents"), &is_dir));
    LP_ASSERT(is_dir);
    LP_ASSERT(!lp_files_exists(abs_path("nope"), NULL));
}

LP_TEST(ensures_the_user_folders) {
    LP_ASSERT(!exists("Desktop"));
    LP_ASSERT_EQ(lp_files_user_dirs_ensure(), 0);
    LP_ASSERT(exists("Desktop"));
    LP_ASSERT(exists("Downloads"));
    LP_ASSERT_EQ(lp_files_user_dirs_ensure(), 0);
}

LP_TEST(detects_text_files) {
    LP_ASSERT(lp_files_is_text(abs_path("Documents/beta.txt")));
    LP_ASSERT(lp_files_is_text(abs_path("Documents/main.c")));
    LP_ASSERT(!lp_files_is_text(abs_path("Documents/photo.png")));
    LP_ASSERT(!lp_files_is_text(abs_path("Documents/song.m4a")));
    write_file("Documents/README", "plain words\n");
    write_bytes("Documents/blob", "ab\0cd", 5);
    write_file("Documents/empty", "");
    LP_ASSERT(lp_files_is_text(abs_path("Documents/README")));
    LP_ASSERT(!lp_files_is_text(abs_path("Documents/blob")));
    LP_ASSERT(lp_files_is_text(abs_path("Documents/empty")));
    LP_ASSERT(!lp_files_is_text(abs_path("Documents/missing")));
    char *text = NULL;
    size_t len = 0;
    LP_ASSERT_EQ(lp_files_read(abs_path("Documents/beta.txt"), &text, &len), 0);
    LP_ASSERT_STR(text, "hello");
    LP_ASSERT_EQ((int)len, 5);
    free(text);
    LP_ASSERT_EQ(lp_files_write(abs_path("Documents/out.txt"), "abc", 3), 0);
    LP_ASSERT_EQ(lp_files_read(abs_path("Documents/out.txt"), &text, &len), 0);
    LP_ASSERT_STR(text, "abc");
    free(text);
}

LP_TEST(creates_unique_folders_and_renames) {
    char name[256];
    LP_ASSERT_EQ(lp_files_mkdir_unique(abs_path("Documents"), name, sizeof name), 0);
    LP_ASSERT_STR(name, "untitled folder");
    LP_ASSERT_EQ(lp_files_mkdir_unique(abs_path("Documents"), name, sizeof name), 0);
    LP_ASSERT_STR(name, "untitled folder 2");
    LP_ASSERT(exists("Documents/untitled folder 2"));
    LP_ASSERT_EQ(lp_files_rename(abs_path("Documents"), "untitled folder 2", "Ideas"), 0);
    LP_ASSERT(exists("Documents/Ideas"));
    LP_ASSERT_EQ(lp_files_rename(abs_path("Documents"), "Ideas", "beta.txt"), -EEXIST);
    LP_ASSERT_EQ(lp_files_rename(abs_path("Documents"), "Ideas", ""), -EINVAL);
    LP_ASSERT_EQ(lp_files_rename(abs_path("Documents"), "Ideas", "a/b"), -EINVAL);
    LP_ASSERT_EQ(lp_files_rename(abs_path("Documents"), "Ideas", ".."), -EINVAL);
    LP_ASSERT_EQ(lp_files_rename(abs_path("Documents"), "Ideas", "Ideas"), 0);
    LP_ASSERT_EQ(lp_files_rename(abs_path("Documents"), "gone", "x"), -ENOENT);
    lp_files_unique_name(abs_path("Documents"), "beta.txt", " copy", name, sizeof name);
    LP_ASSERT_STR(name, "beta copy.txt");
}

LP_TEST(copies_with_finder_conflict_names) {
    char name[256];
    LP_ASSERT_EQ(lp_files_copy(abs_path("Documents/beta.txt"), abs_path("Documents"), name, sizeof name), 0);
    LP_ASSERT_STR(name, "beta copy.txt");
    LP_ASSERT_EQ(lp_files_copy(abs_path("Documents/beta.txt"), abs_path("Documents"), name, sizeof name), 0);
    LP_ASSERT_STR(name, "beta copy 2.txt");
    char *text = NULL;
    lp_files_read(abs_path("Documents/beta copy 2.txt"), &text, NULL);
    LP_ASSERT_STR(text, "hello");
    free(text);
    make_dir("Elsewhere");
    LP_ASSERT_EQ(lp_files_copy(abs_path("Documents/Notes"), abs_path("Elsewhere"), name, sizeof name), 0);
    LP_ASSERT_STR(name, "Notes");
    LP_ASSERT(exists("Elsewhere/Notes/inner.txt"));
    LP_ASSERT_EQ(lp_files_copy(abs_path("Documents/Notes"), abs_path("Elsewhere"), name, sizeof name), 0);
    LP_ASSERT_STR(name, "Notes copy");
    LP_ASSERT_EQ(lp_files_copy(abs_path("Documents/Notes"), abs_path("Documents/Notes"), name, sizeof name), -EINVAL);
    LP_ASSERT_EQ(lp_files_copy(abs_path("Documents/nope"), abs_path("Elsewhere"), name, sizeof name), -ENOENT);
}

LP_TEST(moves_by_rename_or_by_copy_and_delete) {
    make_dir("Target");
    LP_ASSERT_EQ(lp_files_move(abs_path("Documents/alpha.md"), abs_path("Target")), 0);
    LP_ASSERT(exists("Target/alpha.md"));
    LP_ASSERT(!exists("Documents/alpha.md"));
    write_file("Documents/alpha.md", "again");
    LP_ASSERT_EQ(lp_files_move(abs_path("Documents/alpha.md"), abs_path("Target")), -EEXIST);
    LP_ASSERT_EQ(lp_files_move(abs_path("Documents/alpha.md"), abs_path("Documents")), 0); /* same place: nothing */
    LP_ASSERT_EQ(lp_files_move(abs_path("Documents/Notes"), abs_path("Documents/Notes")), -EINVAL);
    lp_files_force_move_fallback = 1;
    LP_ASSERT_EQ(lp_files_move(abs_path("Documents/Notes"), abs_path("Target")), 0);
    lp_files_force_move_fallback = 0;
    LP_ASSERT(exists("Target/Notes/inner.txt"));
    LP_ASSERT(!exists("Documents/Notes"));
    LP_ASSERT_EQ(lp_files_delete_tree(abs_path("Target")), 0);
    LP_ASSERT(!exists("Target"));
    LP_ASSERT_EQ(lp_files_delete_tree(abs_path("Target")), -ENOENT);
}

LP_TEST(trashes_with_a_trashinfo_and_empties) {
    LP_ASSERT_EQ(lp_files_trash(abs_path("Documents/beta.txt")), 0);
    LP_ASSERT(!exists("Documents/beta.txt"));
    LP_ASSERT(exists(".local/share/Trash/files/beta.txt"));
    LP_ASSERT(exists(".local/share/Trash/info/beta.txt.trashinfo"));
    char *info = NULL;
    lp_files_read(abs_path(".local/share/Trash/info/beta.txt.trashinfo"), &info, NULL);
    LP_ASSERT(strncmp(info, "[Trash Info]\nPath=", 18) == 0);
    LP_ASSERT(strstr(info, "/Documents/beta.txt\nDeletionDate=") != NULL);
    free(info);
    write_file("Documents/beta.txt", "second");
    LP_ASSERT_EQ(lp_files_trash(abs_path("Documents/beta.txt")), 0);
    LP_ASSERT(exists(".local/share/Trash/files/beta 2.txt"));
    LP_ASSERT(exists(".local/share/Trash/info/beta 2.txt.trashinfo"));
    make_dir("Documents/Notes");
    write_file("Documents/Notes/inner.txt", "inner");
    lp_files_force_move_fallback = 1;
    LP_ASSERT_EQ(lp_files_trash(abs_path("Documents/Notes")), 0);
    lp_files_force_move_fallback = 0;
    LP_ASSERT(exists(".local/share/Trash/files/Notes/inner.txt"));
    LP_ASSERT_EQ(lp_files_trash(abs_path("Documents/nope")), -ENOENT);
    char trash[1024];
    lp_files_user_dir(LP_USER_TRASH, trash, sizeof trash);
    lp_file_list l = { 0 };
    lp_files_list(trash, 0, &l);
    LP_ASSERT_EQ(l.count, 3);
    lp_files_list_free(&l);
    LP_ASSERT_EQ(lp_files_empty_trash(), 0);
    lp_files_list(trash, 0, &l);
    LP_ASSERT_EQ(l.count, 0);
    lp_files_list_free(&l);
    LP_ASSERT(!exists(".local/share/Trash/info/beta.txt.trashinfo"));
}

LP_TEST(parses_volumes_from_proc_mounts) {
    const char *mounts =
        "sysfs /sys sysfs rw 0 0\n"
        "/dev/vda2 / ext4 rw,relatime 0 0\n"
        "tmpfs /run tmpfs rw 0 0\n"
        "/dev/vda1 /boot/firmware vfat rw 0 0\n"
        "maryos-out /mnt/maryos-out virtiofs rw 0 0\n"
        "/dev/sda1 /media/mary/USB\\040DISK exfat rw 0 0\n"
        "tmpfs /mnt/scratch tmpfs rw 0 0\n"
        "/dev/sda1 /media/mary/USB\\040DISK exfat rw 0 0\n";
    lp_volume v[8];
    int n = lp_files_volumes_parse(mounts, "MaryOS", v, 8);
    LP_ASSERT_EQ(n, 3);
    LP_ASSERT_STR(v[0].name, "MaryOS");
    LP_ASSERT_STR(v[0].path, "/");
    LP_ASSERT_STR(v[1].name, "maryos-out");
    LP_ASSERT_STR(v[1].path, "/mnt/maryos-out");
    LP_ASSERT_STR(v[2].name, "USB DISK");
    LP_ASSERT_STR(v[2].path, "/media/mary/USB DISK");
    LP_ASSERT_EQ(lp_files_volumes_parse("", NULL, v, 8), 1);
    LP_ASSERT_STR(v[0].name, "System");
    LP_ASSERT(lp_files_volumes(v, 8) >= 1);
}

LP_TEST(keeps_a_file_clipboard) {
    lp_file_clipboard *c = lp_files_clipboard_shared();
    const char *paths[2] = { "/a/one", "/a/two" };
    lp_files_clipboard_set(c, paths, 2, 0);
    LP_ASSERT_EQ(c->count, 2);
    LP_ASSERT_STR(c->paths[1], "/a/two");
    LP_ASSERT(!c->cut);
    lp_files_clipboard_set(c, paths, 1, 1);
    LP_ASSERT_EQ(c->count, 1);
    LP_ASSERT(c->cut);
    lp_files_clipboard_clear(c);
    LP_ASSERT_EQ(c->count, 0);
    LP_ASSERT(c->paths == NULL);
}

int main(void) {
    setup();
    LP_RUN(lists_a_directory_sorted_naturally_without_dotfiles);
    LP_RUN(sorts_by_column_with_names_as_the_tie_breaker);
    LP_RUN(compares_names_like_the_finder);
    LP_RUN(formats_sizes_and_dates);
    LP_RUN(path_helpers);
    LP_RUN(ensures_the_user_folders);
    LP_RUN(detects_text_files);
    LP_RUN(creates_unique_folders_and_renames);
    LP_RUN(copies_with_finder_conflict_names);
    LP_RUN(moves_by_rename_or_by_copy_and_delete);
    LP_RUN(trashes_with_a_trashinfo_and_empties);
    LP_RUN(parses_volumes_from_proc_mounts);
    LP_RUN(keeps_a_file_clipboard);
    teardown();
    LP_TEST_MAIN_END();
}
