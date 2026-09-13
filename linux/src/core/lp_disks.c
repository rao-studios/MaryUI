/* Disk Utility's drives (lp_disks.h). */
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/statvfs.h>
#include <unistd.h>

#include "maryui/lp_disks.h"
#include "maryui/lp_files.h"

/* The first line of a small file, trimmed; "" when it cannot be read. */
static void read_line(const char *path, char *out, size_t n) {
    out[0] = 0;
    FILE *f = fopen(path, "r");
    if (!f) return;
    if (fgets(out, (int)n, f)) {
        size_t len = strcspn(out, "\n");
        out[len] = 0;
        while (len > 0 && isspace((unsigned char)out[len - 1])) out[--len] = 0;
        size_t lead = strspn(out, " \t");
        if (lead) memmove(out, out + lead, strlen(out + lead) + 1);
    }
    fclose(f);
}

static uint64_t read_number(const char *path) {
    char text[64];
    read_line(path, text, sizeof text);
    return strtoull(text, NULL, 10);
}

/* Devices that are not drives a person plugged in or booted from. */
static int skipped(const char *name) {
    static const char *const PREFIXES[] = { "loop", "ram", "zram", "dm-", "md", "nbd", "sr", "fd", NULL };
    for (int i = 0; PREFIXES[i]; i++) if (strncmp(name, PREFIXES[i], strlen(PREFIXES[i])) == 0) return 1;
    return 0;
}

/* \x2d and friends, as udev encodes labels in ID_FS_LABEL_ENC. */
static void decode_hex_escapes(const char *in, char *out, size_t n) {
    size_t o = 0;
    for (size_t i = 0; in[i] && o + 1 < n; i++) {
        if (in[i] == '\\' && in[i + 1] == 'x' && isxdigit((unsigned char)in[i + 2]) && isxdigit((unsigned char)in[i + 3])) {
            char hex[3] = { in[i + 2], in[i + 3], 0 };
            out[o++] = (char)strtol(hex, NULL, 16);
            i += 3;
        } else {
            out[o++] = in[i];
        }
    }
    out[o] = 0;
}

typedef struct udev_props { char fs_type[32], label[64], part_name[64], model[64], vendor[64]; } udev_props;

static void read_udev(const char *udev_data, const char *devnum, udev_props *p) {
    memset(p, 0, sizeof *p);
    if (!devnum[0]) return;
    char path[1024], line[512];
    snprintf(path, sizeof path, "%s/b%s", udev_data, devnum);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char label_enc[128] = "", label[64] = "";
    while (fgets(line, sizeof line, f)) {
        line[strcspn(line, "\n")] = 0;
        if (strncmp(line, "E:", 2) != 0) continue;
        const char *kv = line + 2;
        if (strncmp(kv, "ID_FS_TYPE=", 11) == 0) snprintf(p->fs_type, sizeof p->fs_type, "%s", kv + 11);
        else if (strncmp(kv, "ID_FS_LABEL_ENC=", 16) == 0) snprintf(label_enc, sizeof label_enc, "%s", kv + 16);
        else if (strncmp(kv, "ID_FS_LABEL=", 12) == 0) snprintf(label, sizeof label, "%s", kv + 12);
        else if (strncmp(kv, "ID_PART_ENTRY_NAME=", 19) == 0) decode_hex_escapes(kv + 19, p->part_name, sizeof p->part_name);
        else if (strncmp(kv, "ID_MODEL=", 9) == 0) snprintf(p->model, sizeof p->model, "%s", kv + 9);
        else if (strncmp(kv, "ID_VENDOR=", 10) == 0) snprintf(p->vendor, sizeof p->vendor, "%s", kv + 10);
    }
    fclose(f);
    if (label_enc[0]) decode_hex_escapes(label_enc, p->label, sizeof p->label);
    else snprintf(p->label, sizeof p->label, "%s", label);
    for (char *c = p->model; *c; c++) if (*c == '_') *c = ' ';
}

/* /proc/mounts escapes spaces and the like as \040. */
static void unescape_octal(const char *in, char *out, size_t n) {
    size_t o = 0;
    for (size_t i = 0; in[i] && o + 1 < n; i++) {
        if (in[i] == '\\' && in[i + 1] >= '0' && in[i + 1] <= '3' && isdigit((unsigned char)in[i + 2]) && isdigit((unsigned char)in[i + 3])) {
            out[o++] = (char)((in[i + 1] - '0') * 64 + (in[i + 2] - '0') * 8 + (in[i + 3] - '0'));
            i += 3;
        } else {
            out[o++] = in[i];
        }
    }
    out[o] = 0;
}

static int is_system_mount(const char *mount_point) {
    return strcmp(mount_point, "/") == 0 || strncmp(mount_point, "/boot", 5) == 0 || strcmp(mount_point, "/usr") == 0 || strcmp(mount_point, "/var") == 0;
}

static void find_mount(const char *mounts, lp_disk_volume *v) {
    if (!mounts) return;
    const char *line = mounts;
    while (*line) {
        const char *end = strchr(line, '\n');
        size_t len = end ? (size_t)(end - line) : strlen(line);
        char buf[1024], dev[256], mnt[512], type[64];
        if (len >= sizeof buf) len = sizeof buf - 1;
        memcpy(buf, line, len);
        buf[len] = 0;
        if (sscanf(buf, "%255s %511s %63s", dev, mnt, type) == 3 && strcmp(dev, v->device) == 0) {
            unescape_octal(mnt, v->mount_point, sizeof v->mount_point);
            snprintf(v->fs_type, sizeof v->fs_type, "%s", type);
            v->system = is_system_mount(v->mount_point);
            struct statvfs vfs;
            if (statvfs(v->mount_point, &vfs) == 0) v->free_bytes = (uint64_t)vfs.f_bavail * (uint64_t)vfs.f_frsize;
            return;   /* the first mount is where people look for it */
        }
        if (!end) break;
        line = end + 1;
    }
}

static int compare_names(const void *a, const void *b) { return strcmp(a, b); }

static int compare_volumes(const void *a, const void *b) {
    return ((const lp_disk_volume *)a)->number - ((const lp_disk_volume *)b)->number;
}

static void drive_kind(const char *sys_block, const char *name, int removable, char *out, size_t n) {
    char path[1024], target[1024];
    snprintf(path, sizeof path, "%s/%s", sys_block, name);
    ssize_t len = readlink(path, target, sizeof target - 1);
    target[len > 0 ? len : 0] = 0;
    const char *kind = removable ? "External" : "Internal";
    if (strstr(target, "/usb")) kind = "USB";
    else if (strstr(target, "/mmc")) kind = "SD card";
    else if (strstr(target, "/nvme")) kind = "NVMe";
    else if (strstr(target, "/virtio")) kind = "Virtual";
    else if (strstr(target, "/ata")) kind = "SATA";
    snprintf(out, n, "%s", kind);
}

static void read_drive(const char *sys_block, const char *udev_data, const char *mounts, const char *name, lp_disk_drive *drv) {
    char path[1024], devnum[32];
    memset(drv, 0, sizeof *drv);
    snprintf(drv->device, sizeof drv->device, "/dev/%s", name);
    snprintf(path, sizeof path, "%s/%s/size", sys_block, name);
    drv->size = read_number(path) * 512;   /* sysfs counts 512-byte sectors whatever the drive's own */
    snprintf(path, sizeof path, "%s/%s/removable", sys_block, name);
    drv->removable = read_number(path) != 0;
    snprintf(path, sizeof path, "%s/%s/ro", sys_block, name);
    drv->read_only = read_number(path) != 0;
    drive_kind(sys_block, name, drv->removable, drv->kind, sizeof drv->kind);
    snprintf(path, sizeof path, "%s/%s/dev", sys_block, name);
    read_line(path, devnum, sizeof devnum);
    udev_props whole;
    read_udev(udev_data, devnum, &whole);

    char vendor[64], model[64];
    snprintf(path, sizeof path, "%s/%s/device/vendor", sys_block, name);
    read_line(path, vendor, sizeof vendor);
    snprintf(path, sizeof path, "%s/%s/device/model", sys_block, name);
    read_line(path, model, sizeof model);
    if (!model[0]) { snprintf(path, sizeof path, "%s/%s/device/name", sys_block, name); read_line(path, model, sizeof model); }   /* an SD card */
    if (!model[0]) snprintf(model, sizeof model, "%s", whole.model);
    if (!vendor[0]) snprintf(vendor, sizeof vendor, "%s", whole.vendor);
    if (vendor[0] && model[0] && strncmp(model, vendor, strlen(vendor)) != 0) snprintf(drv->name, sizeof drv->name, "%s %s", vendor, model);
    else if (model[0]) snprintf(drv->name, sizeof drv->name, "%s", model);
    else if (strcmp(drv->kind, "Virtual") == 0) snprintf(drv->name, sizeof drv->name, "Virtual Disk");
    else if (strcmp(drv->kind, "SD card") == 0) snprintf(drv->name, sizeof drv->name, "SD Card");
    else if (strcmp(drv->kind, "USB") == 0) snprintf(drv->name, sizeof drv->name, "USB Drive");
    else snprintf(drv->name, sizeof drv->name, "Disk");

    /* its partitions: the subdirectories that have a "partition" file */
    snprintf(path, sizeof path, "%s/%s", sys_block, name);
    DIR *dir = opendir(path);
    struct dirent *e;
    while (dir && (e = readdir(dir)) && drv->nvolumes < LP_DISKS_MAX_VOLUMES) {
        if (strncmp(e->d_name, name, strlen(name)) != 0) continue;
        char part[1024];
        snprintf(part, sizeof part, "%s/%s/%s/partition", sys_block, name, e->d_name);
        if (access(part, R_OK) != 0) continue;
        lp_disk_volume *v = &drv->volumes[drv->nvolumes++];
        v->number = (int)read_number(part);
        snprintf(v->device, sizeof v->device, "/dev/%s", e->d_name);
        snprintf(part, sizeof part, "%s/%s/%s/size", sys_block, name, e->d_name);
        v->size = read_number(part) * 512;
        snprintf(part, sizeof part, "%s/%s/%s/dev", sys_block, name, e->d_name);
        read_line(part, devnum, sizeof devnum);
        udev_props props;
        read_udev(udev_data, devnum, &props);
        snprintf(v->fs_type, sizeof v->fs_type, "%s", props.fs_type);
        snprintf(v->label, sizeof v->label, "%s", props.label[0] ? props.label : props.part_name);
    }
    if (dir) closedir(dir);
    if (drv->nvolumes == 0 && whole.fs_type[0]) {
        /* a filesystem across the whole drive, as many USB sticks come */
        lp_disk_volume *v = &drv->volumes[drv->nvolumes++];
        memcpy(v->device, drv->device, sizeof v->device);   /* the same size; snprintf would trip -Wrestrict inside one struct */
        v->size = drv->size;
        snprintf(v->fs_type, sizeof v->fs_type, "%s", whole.fs_type);
        snprintf(v->label, sizeof v->label, "%s", whole.label);
    }
    qsort(drv->volumes, (size_t)drv->nvolumes, sizeof drv->volumes[0], compare_volumes);
    for (int i = 0; i < drv->nvolumes; i++) {
        find_mount(mounts, &drv->volumes[i]);
        if (drv->volumes[i].system) drv->system = 1;
    }
}

int lp_disks_read_from(const char *sys_block, const char *udev_data, const char *mounts, lp_disks *out) {
    memset(out, 0, sizeof *out);
    DIR *dir = opendir(sys_block);
    if (!dir) return -errno;
    char names[64][64];
    int n = 0;
    struct dirent *e;
    while ((e = readdir(dir)) && n < 64) {
        if (e->d_name[0] == '.' || skipped(e->d_name)) continue;
        snprintf(names[n++], sizeof names[0], "%s", e->d_name);
    }
    closedir(dir);
    qsort(names, (size_t)n, sizeof names[0], compare_names);
    for (int i = 0; i < n && out->count < LP_DISKS_MAX_DRIVES; i++) {
        lp_disk_drive *drv = &out->drives[out->count];
        read_drive(sys_block, udev_data, mounts, names[i], drv);
        if (drv->size > 0) out->count++;   /* an empty card reader is not a drive to show */
    }
    return 0;
}

int lp_disks_read(lp_disks *out) {
    char *mounts = NULL;
    size_t len = 0;
    if (lp_files_read("/proc/mounts", &mounts, &len) != 0) mounts = NULL;
    int rc = lp_disks_read_from("/sys/block", "/run/udev/data", mounts ? mounts : "", out);
    free(mounts);
    return rc;
}

void lp_disks_volume_name(const lp_disk_drive *drive, const lp_disk_volume *v, char *out, size_t n) {
    if (v->label[0]) snprintf(out, n, "%s", v->label);
    else if (v->number > 0) snprintf(out, n, "Partition %d", v->number);
    else snprintf(out, n, "%s", drive->name);
}

int lp_disks_action_argv(enum lp_disk_action action, const char *device, const char **argv, int max) {
    const char *verb = action == LP_DISK_MOUNT ? "mount" : action == LP_DISK_UNMOUNT ? "unmount" : "power-off";
    const char *const parts[] = { "udisksctl", verb, "--no-user-interaction", "-b", device, NULL };
    int n = 0;
    for (; parts[n] && n + 1 < max; n++) argv[n] = parts[n];
    if (max > 0) argv[n] = NULL;
    return n;
}
