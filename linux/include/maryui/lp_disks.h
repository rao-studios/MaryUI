/* Disk Utility's model: the drives the kernel knows (/sys/block), what their
 * partitions hold (udev's database under /run/udev/data), and where they are
 * mounted (/proc/mounts), plus the udisksctl command that mounts, unmounts or
 * ejects one — run as an lp_job, so udisks2's polkit rules decide who may.
 * Every path is an argument, so the tests read fixtures and run on a Mac.
 * Linux only in use (PARITY D15). */
#ifndef MARYUI_LP_DISKS_H
#define MARYUI_LP_DISKS_H

#include <stddef.h>
#include <stdint.h>

#define LP_DISKS_MAX_DRIVES 16
#define LP_DISKS_MAX_VOLUMES 16

typedef struct lp_disk_volume {
    char device[64];            /* "/dev/vda2" */
    char label[64];             /* the filesystem's label, else the partition's name; "" when neither */
    char fs_type[32];           /* "ext4", "vfat", "exfat"; "" when nothing recognisable is on it */
    int number;                 /* the partition number; 0 for a filesystem across the whole drive */
    uint64_t size;
    char mount_point[256];      /* "" when not mounted */
    uint64_t free_bytes;        /* when mounted */
    int system;                 /* mounted at /, /boot…, /usr or /var: never offered for unmounting */
} lp_disk_volume;

typedef struct lp_disk_drive {
    char name[96];              /* "SanDisk Ultra", "Virtual Disk", "SD64G" */
    char device[64];            /* "/dev/sda" */
    char kind[24];              /* "USB", "SD card", "NVMe", "SATA", "Virtual", "Internal", "External" */
    uint64_t size;
    int removable, read_only;
    int system;                 /* one of its volumes is a system one */
    lp_disk_volume volumes[LP_DISKS_MAX_VOLUMES];
    int nvolumes;
} lp_disk_drive;

typedef struct lp_disks {
    lp_disk_drive drives[LP_DISKS_MAX_DRIVES];
    int count;
} lp_disks;

enum lp_disk_action { LP_DISK_MOUNT, LP_DISK_UNMOUNT, LP_DISK_EJECT };

/* Reads the drives under sys_block, their filesystems from udev_data, and mounts (the text of /proc/mounts). 0 or -errno. */
int lp_disks_read_from(const char *sys_block, const char *udev_data, const char *mounts, lp_disks *out);
/* The same from /sys/block, /run/udev/data and /proc/mounts. */
int lp_disks_read(lp_disks *out);
/* What a volume is called: its label, else "Partition N", else the drive's name. */
void lp_disks_volume_name(const lp_disk_drive *drive, const lp_disk_volume *v, char *out, size_t n);
/* The udisksctl command for an action on device (a volume for mount/unmount, a drive for eject),
 * NULL-terminated into argv; returns the argument count. */
int lp_disks_action_argv(enum lp_disk_action action, const char *device, const char **argv, int max);

#endif
