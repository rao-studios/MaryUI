/* Disk Utility's drives, read from a fixture of sysfs, udev's database and
 * /proc/mounts: a virtual disk with its two system partitions, a USB stick with
 * a label and an escaped mount point, a blank SD card, and the devices that are
 * not drives. Runs on a Mac as well as Linux. */
#define _DARWIN_C_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "lp_test.h"
#include "maryui/lp_disks.h"
#include "maryui/lp_files.h"

static char root[512], sys_block[600], udev[600];

static void mkdirs(const char *rel) {
    char path[1024];
    snprintf(path, sizeof path, "%s/%s", root, rel);
    lp_files_mkdir_p(path);
}

static void put(const char *rel, const char *text) {
    char path[1024];
    snprintf(path, sizeof path, "%s/%s", root, rel);
    char dir[1024];
    snprintf(dir, sizeof dir, "%s", path);
    char *slash = strrchr(dir, '/');
    if (slash) { *slash = 0; lp_files_mkdir_p(dir); }
    FILE *f = fopen(path, "w");
    if (!f) { perror(path); exit(1); }
    fputs(text, f);
    fclose(f);
}

/* sys/block/<name> is a link into sys/devices/<place>/<name>, as in sysfs. */
static void drive(const char *name, const char *place) {
    char rel[512], link_path[1024], target[1024];
    snprintf(rel, sizeof rel, "sys/devices/%s/%s", place, name);
    mkdirs(rel);
    mkdirs("sys/block");
    snprintf(link_path, sizeof link_path, "%s/sys/block/%s", root, name);
    snprintf(target, sizeof target, "../devices/%s/%s", place, name);
    if (symlink(target, link_path) != 0) perror("symlink");
}

static void write_fixture(void) {
    drive("vda", "pci0000:00/0000:00:05.0/virtio2/block");
    put("sys/block/vda/size", "16777216\n");
    put("sys/block/vda/removable", "0\n");
    put("sys/block/vda/ro", "0\n");
    put("sys/block/vda/dev", "253:0\n");
    put("sys/block/vda/vda1/partition", "1\n");
    put("sys/block/vda/vda1/size", "1048576\n");
    put("sys/block/vda/vda1/dev", "253:1\n");
    put("sys/block/vda/vda2/partition", "2\n");
    put("sys/block/vda/vda2/size", "15720415\n");
    put("sys/block/vda/vda2/dev", "253:2\n");
    put("sys/block/vda/vda3/partition", "3\n");
    put("sys/block/vda/vda3/size", "2048\n");
    put("sys/block/vda/vda3/dev", "253:3\n");
    put("udev/b253:1", "S:disk/by-label/MARYOS\nE:ID_FS_TYPE=vfat\nE:ID_FS_LABEL=MARYOS\n");
    put("udev/b253:2", "E:ID_FS_TYPE=ext4\nE:ID_FS_LABEL=maryos_root\nE:ID_FS_LABEL_ENC=maryos\\x2droot\n");

    drive("sda", "platform/usb1/1-1/1-1:1.0/host0/target0:0:0/0:0:0:0/block");
    put("sys/block/sda/size", "62521344\n");
    put("sys/block/sda/removable", "1\n");
    put("sys/block/sda/ro", "0\n");
    put("sys/block/sda/dev", "8:0\n");
    put("sys/block/sda/device/vendor", "SanDisk \n");
    put("sys/block/sda/device/model", "Ultra           \n");
    put("sys/block/sda/sda1/partition", "1\n");
    put("sys/block/sda/sda1/size", "62519296\n");
    put("sys/block/sda/sda1/dev", "8:1\n");
    put("udev/b8:1", "E:ID_FS_TYPE=exfat\nE:ID_FS_LABEL=PHOTOS_DISK\nE:ID_FS_LABEL_ENC=PHOTOS\\x20DISK\n");

    drive("mmcblk0", "platform/mmc0/mmc_host/mmc0/mmc0:0001/block");
    put("sys/block/mmcblk0/size", "124735488\n");
    put("sys/block/mmcblk0/removable", "0\n");
    put("sys/block/mmcblk0/dev", "179:0\n");
    put("sys/block/mmcblk0/device/name", "SD64G\n");

    drive("sdb", "platform/usb1/1-2/block");         /* a card reader with no card */
    put("sys/block/sdb/size", "0\n");
    put("sys/block/sdb/removable", "1\n");
    drive("loop0", "virtual/block");
    put("sys/block/loop0/size", "2048\n");
    drive("zram0", "virtual/block");
    put("sys/block/zram0/size", "2048\n");
}

static const char MOUNTS[] =
    "/dev/vda2 / ext4 rw,relatime 0 0\n"
    "proc /proc proc rw,nosuid 0 0\n"
    "/dev/vda1 /boot/firmware vfat rw,relatime 0 0\n"
    "/dev/sda1 /media/mary/PHOTOS\\040DISK exfat rw,nosuid,nodev 0 0\n";

LP_TEST(reads_drives_and_skips_what_is_not_one) {
    lp_disks disks;
    LP_ASSERT_EQ(lp_disks_read_from(sys_block, udev, MOUNTS, &disks), 0);
    LP_ASSERT_EQ(disks.count, 3);   /* mmcblk0, sda, vda — not sdb (empty), loop0 or zram0 */
    LP_ASSERT_STR(disks.drives[0].device, "/dev/mmcblk0");
    LP_ASSERT_STR(disks.drives[1].device, "/dev/sda");
    LP_ASSERT_STR(disks.drives[2].device, "/dev/vda");
}

LP_TEST(names_each_drive_by_what_it_is) {
    lp_disks disks;
    lp_disks_read_from(sys_block, udev, MOUNTS, &disks);
    const lp_disk_drive *card = &disks.drives[0], *stick = &disks.drives[1], *vm = &disks.drives[2];
    LP_ASSERT_STR(card->name, "SD64G");
    LP_ASSERT_STR(card->kind, "SD card");
    LP_ASSERT_EQ(card->nvolumes, 0);   /* nothing on it yet */
    LP_ASSERT_STR(stick->name, "SanDisk Ultra");
    LP_ASSERT_STR(stick->kind, "USB");
    LP_ASSERT(stick->removable);
    LP_ASSERT(!stick->system);
    LP_ASSERT_STR(vm->name, "Virtual Disk");
    LP_ASSERT_STR(vm->kind, "Virtual");
    LP_ASSERT(vm->size == 16777216ULL * 512);
    LP_ASSERT(vm->system);
}

LP_TEST(reads_each_volume_and_where_it_is_mounted) {
    lp_disks disks;
    lp_disks_read_from(sys_block, udev, MOUNTS, &disks);
    const lp_disk_drive *vm = &disks.drives[2], *stick = &disks.drives[1];
    LP_ASSERT_EQ(vm->nvolumes, 3);
    const lp_disk_volume *boot = &vm->volumes[0], *rootfs = &vm->volumes[1], *spare = &vm->volumes[2];
    LP_ASSERT_EQ(boot->number, 1);
    LP_ASSERT_STR(boot->label, "MARYOS");
    LP_ASSERT_STR(boot->fs_type, "vfat");
    LP_ASSERT_STR(boot->mount_point, "/boot/firmware");
    LP_ASSERT(boot->system);
    LP_ASSERT_STR(rootfs->label, "maryos-root");        /* from the encoded label, not the underscored one */
    LP_ASSERT_STR(rootfs->mount_point, "/");
    LP_ASSERT(rootfs->system);
    LP_ASSERT(rootfs->size == 15720415ULL * 512);
    LP_ASSERT_STR(spare->mount_point, "");
    LP_ASSERT_STR(spare->fs_type, "");
    LP_ASSERT(!spare->system);
    char name[64];
    lp_disks_volume_name(vm, spare, name, sizeof name);
    LP_ASSERT_STR(name, "Partition 3");
    const lp_disk_volume *photos = &stick->volumes[0];
    LP_ASSERT_STR(photos->label, "PHOTOS DISK");
    LP_ASSERT_STR(photos->mount_point, "/media/mary/PHOTOS DISK");   /* \040 is a space */
    LP_ASSERT_STR(photos->fs_type, "exfat");
    LP_ASSERT(!photos->system);
}

LP_TEST(builds_the_udisksctl_command_for_each_action) {
    const char *argv[8];
    LP_ASSERT_EQ(lp_disks_action_argv(LP_DISK_UNMOUNT, "/dev/sda1", argv, 8), 5);
    LP_ASSERT_STR(argv[0], "udisksctl");
    LP_ASSERT_STR(argv[1], "unmount");
    LP_ASSERT_STR(argv[2], "--no-user-interaction");
    LP_ASSERT_STR(argv[4], "/dev/sda1");
    LP_ASSERT(argv[5] == NULL);
    lp_disks_action_argv(LP_DISK_MOUNT, "/dev/sda1", argv, 8);
    LP_ASSERT_STR(argv[1], "mount");
    lp_disks_action_argv(LP_DISK_EJECT, "/dev/sda", argv, 8);
    LP_ASSERT_STR(argv[1], "power-off");
}

LP_TEST(a_missing_sysfs_is_an_error) {
    lp_disks disks;
    char missing[700];
    snprintf(missing, sizeof missing, "%s/nowhere", root);
    LP_ASSERT(lp_disks_read_from(missing, udev, MOUNTS, &disks) < 0);
    LP_ASSERT_EQ(disks.count, 0);
}

int main(void) {
    const char *tmp = getenv("TMPDIR");
    snprintf(root, sizeof root, "%s/lp_disks_XXXXXX", tmp && *tmp ? tmp : "/tmp");
    if (!mkdtemp(root)) return 1;
    snprintf(sys_block, sizeof sys_block, "%s/sys/block", root);
    snprintf(udev, sizeof udev, "%s/udev", root);
    write_fixture();
    LP_RUN(reads_drives_and_skips_what_is_not_one);
    LP_RUN(names_each_drive_by_what_it_is);
    LP_RUN(reads_each_volume_and_where_it_is_mounted);
    LP_RUN(builds_the_udisksctl_command_for_each_action);
    LP_RUN(a_missing_sysfs_is_an_error);
    lp_files_delete_tree(root);
    LP_TEST_MAIN_END();
}
