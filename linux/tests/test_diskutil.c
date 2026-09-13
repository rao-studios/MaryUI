/* Disk Utility, driven headlessly over fixture drives: what each selection
 * may do, Unmount and Mount as udisksctl commands with the job's result shown
 * and the drives reread, udisks2's refusals trimmed to their reason, Eject as
 * one script over the mounted volumes, nothing offered for the system's own
 * volumes, and a DRAW pass into a real surface. The job runner is stood in
 * for, so nothing is ever mounted. Runs on a Mac as well as Linux. */
#define _DARWIN_C_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "lp_test.h"
#include "maryui/lp_desktop.h"
#include "maryui/lp_files.h"
#include "maryui/lp_job.h"

static char root[512], sys_block[600], udev[600], mounts[600], media[600];
static lp_desktop d;

static char ran[1024];
static lp_job_done_fn pending_done;
static void *pending_user;
static int runs;

static lp_job *fake_run(lp_desktop *desk, const char *const *argv, lp_job_done_fn done, void *user) {
    ran[0] = 0;
    for (int i = 0; argv[i]; i++) {
        size_t len = strlen(ran);
        snprintf(ran + len, sizeof ran - len, "%s%s", i ? " " : "", argv[i]);
    }
    pending_done = done;
    pending_user = user;
    runs++;
    return (lp_job *)&pending_done;   /* a token: the test finishes the job itself */
}

static void finish_job(int status, const char *output) {
    lp_job_done_fn done = pending_done;
    pending_done = NULL;
    done(status, output, pending_user);
}

static void put(const char *rel, const char *text) {
    char path[1024], dir[1024];
    snprintf(path, sizeof path, "%s/%s", root, rel);
    snprintf(dir, sizeof dir, "%s", path);
    char *slash = strrchr(dir, '/');
    if (slash) { *slash = 0; lp_files_mkdir_p(dir); }
    FILE *f = fopen(path, "w");
    if (!f) { perror(path); exit(1); }
    fputs(text, f);
    fclose(f);
}

static void drive_link(const char *name, const char *place) {
    char rel[512], link_path[1024], target[1024];
    snprintf(rel, sizeof rel, "%s/sys/devices/%s/%s", root, place, name);
    lp_files_mkdir_p(rel);
    snprintf(rel, sizeof rel, "%s/sys/block", root);
    lp_files_mkdir_p(rel);
    snprintf(link_path, sizeof link_path, "%s/sys/block/%s", root, name);
    snprintf(target, sizeof target, "../devices/%s/%s", place, name);
    if (symlink(target, link_path) != 0) perror("symlink");
}

static void write_mounts(int stick_mounted) {
    char text[1200];
    snprintf(text, sizeof text, "/dev/vda2 / ext4 rw 0 0\n/dev/vda1 /boot/firmware vfat rw 0 0\n%s%s%s",
             stick_mounted ? "/dev/sda1 " : "", stick_mounted ? media : "", stick_mounted ? " exfat rw 0 0\n" : "");
    put("mounts", text);
}

static void write_fixture(void) {
    drive_link("vda", "pci0000:00/virtio2/block");
    put("sys/block/vda/size", "16777216\n");
    put("sys/block/vda/dev", "253:0\n");
    put("sys/block/vda/vda1/partition", "1\n");
    put("sys/block/vda/vda1/size", "1048576\n");
    put("sys/block/vda/vda1/dev", "253:1\n");
    put("sys/block/vda/vda2/partition", "2\n");
    put("sys/block/vda/vda2/size", "15720415\n");
    put("sys/block/vda/vda2/dev", "253:2\n");
    put("udev/b253:1", "E:ID_FS_TYPE=vfat\nE:ID_FS_LABEL=MARYOS\n");
    put("udev/b253:2", "E:ID_FS_TYPE=ext4\nE:ID_FS_LABEL=maryos-root\n");
    drive_link("sda", "platform/usb1/1-1/host0/block");
    put("sys/block/sda/size", "62521344\n");
    put("sys/block/sda/removable", "1\n");
    put("sys/block/sda/dev", "8:0\n");
    put("sys/block/sda/device/vendor", "SanDisk\n");
    put("sys/block/sda/device/model", "Ultra\n");
    put("sys/block/sda/sda1/partition", "1\n");
    put("sys/block/sda/sda1/size", "62519296\n");
    put("sys/block/sda/sda1/dev", "8:1\n");
    put("udev/b8:1", "E:ID_FS_TYPE=exfat\nE:ID_FS_LABEL=PHOTOS\n");
    lp_files_mkdir_p(media);
    write_mounts(1);
}

static void *open_utility(char *id) {
    lp_desktop_init(&d, LP_RECT(0, 0, 1280, 800), NULL);
    lp_desktop_register_builtin_apps(&d);
    if (lp_desktop_open_app_with(&d, "diskutil", NULL, NULL, id) != 1) return NULL;
    void *u = lp_desktop_instance(&d, id)->state;
    lp_diskutil_set_runner(u, fake_run);
    lp_diskutil_set_paths(u, sys_block, udev, mounts);
    runs = 0;
    return u;
}

LP_TEST(offers_only_what_each_selection_allows) {
    char id[12];
    void *u = open_utility(id);
    LP_ASSERT(u != NULL);
    LP_ASSERT_STR(lp_diskutil_selected(u), "/dev/sda1");     /* the external volume first */
    LP_ASSERT(lp_diskutil_can(u, LP_DISKUTIL_UNMOUNT));
    LP_ASSERT(!lp_diskutil_can(u, LP_DISKUTIL_MOUNT));
    LP_ASSERT(lp_diskutil_can(u, LP_DISKUTIL_EJECT));
    LP_ASSERT(lp_diskutil_can(u, LP_DISKUTIL_SHOW_IN_FINDER));
    lp_diskutil_select(u, "/dev/vda2");
    LP_ASSERT(!lp_diskutil_can(u, LP_DISKUTIL_UNMOUNT));     /* the system runs from it */
    LP_ASSERT(!lp_diskutil_can(u, LP_DISKUTIL_EJECT));
    lp_diskutil_select(u, "/dev/vda");
    LP_ASSERT(!lp_diskutil_can(u, LP_DISKUTIL_MOUNT));       /* a drive is not a volume */
    LP_ASSERT(!lp_diskutil_can(u, LP_DISKUTIL_EJECT));
    lp_desktop_close_window(&d, id);
}

LP_TEST(unmounts_and_mounts_through_udisksctl) {
    char id[12];
    void *u = open_utility(id);
    lp_desktop_run_command(&d, LP_CMD_APP, LP_DISKUTIL_UNMOUNT);
    LP_ASSERT_EQ(runs, 1);
    LP_ASSERT_STR(ran, "udisksctl unmount --no-user-interaction -b /dev/sda1");
    LP_ASSERT(lp_diskutil_busy(u));
    LP_ASSERT(!lp_diskutil_can(u, LP_DISKUTIL_EJECT));       /* one job at a time */
    write_mounts(0);
    finish_job(0, "Unmounted /dev/sda1.\n");
    LP_ASSERT(!lp_diskutil_busy(u));
    LP_ASSERT_STR(lp_diskutil_status(u), "Unmounted “PHOTOS”.");
    LP_ASSERT(lp_diskutil_can(u, LP_DISKUTIL_MOUNT));        /* reread: it is no longer mounted */
    lp_desktop_run_command(&d, LP_CMD_APP, LP_DISKUTIL_MOUNT);
    LP_ASSERT_STR(ran, "udisksctl mount --no-user-interaction -b /dev/sda1");
    finish_job(1, "Error mounting /dev/sda1: GDBus.Error:org.freedesktop.UDisks2.Error.NotAuthorizedCanObtain: Not authorized to perform operation\n");
    LP_ASSERT_STR(lp_diskutil_status(u), "Could not mount “PHOTOS”: Not authorized to perform operation");
    write_mounts(1);
    lp_diskutil_refresh(u);
    lp_desktop_close_window(&d, id);
}

LP_TEST(ejects_by_unmounting_then_powering_off) {
    char id[12];
    void *u = open_utility(id);
    lp_desktop_run_command(&d, LP_CMD_APP, LP_DISKUTIL_EJECT);
    LP_ASSERT_EQ(runs, 1);
    LP_ASSERT(strncmp(ran, "/bin/sh -c ", 11) == 0);
    LP_ASSERT(strstr(ran, "udisksctl power-off") != NULL);
    LP_ASSERT(strstr(ran, " eject /dev/sda /dev/sda1") != NULL);   /* the drive, then each mounted volume */
    finish_job(0, "");
    LP_ASSERT_STR(lp_diskutil_status(u), "Ejected “SanDisk Ultra”.");
    lp_desktop_close_window(&d, id);
}

LP_TEST(shows_a_mounted_volume_in_the_finder) {
    char id[12];
    void *u = open_utility(id);
    int before = d.wm.count;
    lp_desktop_run_command(&d, LP_CMD_APP, LP_DISKUTIL_SHOW_IN_FINDER);
    LP_ASSERT_EQ(d.wm.count, before + 1);
    LP_ASSERT_STR(d.wm.windows[d.wm.count - 1].app_id, "finder");
    LP_ASSERT_STR(lp_diskutil_status(u), "");
}

LP_TEST(paints_its_panes) {
    char id[12];
    void *u = open_utility(id);
    cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 760, 438);
    cairo_t *cr = cairo_create(s);
    lp_ctx ctx = { 0 };
    ctx.settings = &d.settings;
    lp_input in = { .mx = -1, .my = -1 };
    for (int pass = 0; pass < 2; pass++) {
        lp_ctx_begin(&ctx, pass ? LP_PASS_DRAW : LP_PASS_EVENT, pass ? cr : NULL, &in, LP_RECT(0, 0, 760, 438), 1000 + pass);
        lp_app_diskutil.paint(u, &ctx, LP_RECT(0, 0, 760, 438), &d);
        lp_ctx_end(&ctx);
    }
    lp_diskutil_select(u, "/dev/vda");
    lp_ctx_begin(&ctx, LP_PASS_DRAW, cr, &in, LP_RECT(0, 0, 760, 438), 1100);
    lp_app_diskutil.paint(u, &ctx, LP_RECT(0, 0, 760, 438), &d);
    lp_ctx_end(&ctx);
    cairo_surface_flush(s);
    uint32_t px;
    memcpy(&px, cairo_image_surface_get_data(s) + 200 * cairo_image_surface_get_stride(s) + 4 * 400, 4);
    LP_ASSERT((px >> 24) == 255);   /* the detail pane is painted */
    cairo_destroy(cr);
    cairo_surface_destroy(s);
    lp_desktop_close_window(&d, id);
}

int main(void) {
    const char *tmp = getenv("TMPDIR");
    snprintf(root, sizeof root, "%s/lp_diskutil_XXXXXX", tmp && *tmp ? tmp : "/tmp");
    if (!mkdtemp(root)) return 1;
    snprintf(sys_block, sizeof sys_block, "%s/sys/block", root);
    snprintf(udev, sizeof udev, "%s/udev", root);
    snprintf(mounts, sizeof mounts, "%s/mounts", root);
    snprintf(media, sizeof media, "%s/media/PHOTOS", root);
    char config[600];
    snprintf(config, sizeof config, "%s/config", root);
    setenv("XDG_CONFIG_HOME", config, 1);
    setenv("HOME", root, 1);
    write_fixture();
    LP_RUN(offers_only_what_each_selection_allows);
    LP_RUN(unmounts_and_mounts_through_udisksctl);
    LP_RUN(ejects_by_unmounting_then_powering_off);
    LP_RUN(shows_a_mounted_volume_in_the_finder);
    LP_RUN(paints_its_panes);
    lp_files_delete_tree(root);
    LP_TEST_MAIN_END();
}
