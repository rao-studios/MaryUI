/* The host's clipboard, when MaryOS runs in `maryos vm run` on a Mac (PARITY D22). The Mac writes
 * every change of its pasteboard, while the VM's window is in front, to a virtio console port named
 * org.maryos.clipboard: a u32 little-endian byte count, then that much UTF-8. The desktop reads the
 * port and makes each message the text clipboard everything pastes from (lp_text_clipboard_shared),
 * so a key copied on the Mac pastes into System Settings. Host to guest only. On a Pi, or in a VM
 * started without it, there is no port and nothing happens. */
#ifndef MARYUI_LP_CLIPBOARD_PORT_H
#define MARYUI_LP_CLIPBOARD_PORT_H

#include <stddef.h>

struct lp_desktop;
struct lp_source;

#define LP_CLIPBOARD_PORT "/dev/virtio-ports/org.maryos.clipboard"
#define LP_CLIPBOARD_FRAME_MAX (1u << 20)   /* a longer message is skipped whole */
#define LP_CLIPBOARD_RETRY_MS 5000          /* when the host's side goes away */

typedef struct lp_clipboard_port {
    struct lp_desktop *desk;
    char path[256];
    int fd;                                 /* -1 while closed */
    struct lp_source *source, *retry;
    unsigned char header[4];
    size_t header_len;
    char *text;                             /* the message being read */
    size_t want, have;
    size_t skip;                            /* bytes of an oversized message still to drop */
    int messages;                           /* messages taken */
} lp_clipboard_port;

void lp_clipboard_port_init(lp_clipboard_port *p);
/* Opens the port (non-blocking) and reads it on the desktop's event loop. 0; -ENOENT when there is
 * no such port (not in a VM); -ENOSYS when the host has no event sources; -errno. */
int lp_clipboard_port_open(lp_clipboard_port *p, struct lp_desktop *d, const char *path);
void lp_clipboard_port_close(lp_clipboard_port *p);
/* Bytes from the port, split anywhere: each whole message becomes the text clipboard. Returns how many
 * messages these bytes completed. */
int lp_clipboard_port_feed(lp_clipboard_port *p, const char *bytes, size_t n);

#endif
