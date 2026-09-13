/* The host's clipboard through its port (PARITY D22): messages split anywhere, several in one read,
 * an oversized one skipped whole, an empty one ignored, and a named pipe standing in for the virtio
 * port on the poll loop — a message read, the host side closing and the port looked for again. Runs
 * on a Mac as well as Linux. */
#define _DARWIN_C_SOURCE 1
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "lp_test.h"
#include "lp_test_loop.h"
#include "maryui/components/lp_text_area.h"
#include "maryui/lp_clipboard_port.h"
#include "maryui/lp_desktop.h"

static size_t message(char *out, const char *text) {
    uint32_t n = (uint32_t)strlen(text);
    out[0] = (char)(n & 0xff);
    out[1] = (char)(n >> 8 & 0xff);
    out[2] = (char)(n >> 16 & 0xff);
    out[3] = (char)(n >> 24 & 0xff);
    memcpy(out + 4, text, n);
    return 4 + n;
}

static int clipboard_is(const char *want) {
    const lp_text_clipboard *c = lp_text_clipboard_shared();
    return c->text && c->len == (int)strlen(want) && memcmp(c->text, want, (size_t)c->len) == 0;
}

LP_TEST(a_message_split_anywhere_becomes_the_clipboard) {
    lp_clipboard_port p;
    lp_clipboard_port_init(&p);
    char bytes[64];
    size_t n = message(bytes, "sk-from-the-mac");
    int taken = 0;
    for (size_t i = 0; i < n; i++) taken += lp_clipboard_port_feed(&p, bytes + i, 1);
    LP_ASSERT_EQ(taken, 1);
    LP_ASSERT(clipboard_is("sk-from-the-mac"));

    char two[64];
    size_t m = message(two, "first");
    m += message(two + m, "second");
    LP_ASSERT_EQ(lp_clipboard_port_feed(&p, two, m), 2);
    LP_ASSERT(clipboard_is("second"));
    lp_clipboard_port_close(&p);
}

LP_TEST(an_oversized_message_is_skipped_and_an_empty_one_ignored) {
    lp_clipboard_port p;
    lp_clipboard_port_init(&p);
    char *big = malloc(LP_CLIPBOARD_FRAME_MAX + 64);
    uint32_t n = LP_CLIPBOARD_FRAME_MAX + 10;
    big[0] = (char)(n & 0xff);
    big[1] = (char)(n >> 8 & 0xff);
    big[2] = (char)(n >> 16 & 0xff);
    big[3] = (char)(n >> 24 & 0xff);
    memset(big + 4, 'x', n);
    lp_text_clipboard_set(lp_text_clipboard_shared(), "kept", 4);
    LP_ASSERT_EQ(lp_clipboard_port_feed(&p, big, 4 + (size_t)n), 0);
    LP_ASSERT(clipboard_is("kept"));
    free(big);
    char empty[4] = { 0, 0, 0, 0 }, ok[16];
    LP_ASSERT_EQ(lp_clipboard_port_feed(&p, empty, 4), 0);
    size_t k = message(ok, "ok");
    LP_ASSERT_EQ(lp_clipboard_port_feed(&p, ok, k), 1);
    LP_ASSERT(clipboard_is("ok"));
    LP_ASSERT_EQ(p.messages, 1);
    lp_clipboard_port_close(&p);
}

static int got_it(void) { return clipboard_is("pasted from the host"); }

LP_TEST(the_port_is_read_on_the_event_loop_and_looked_for_again) {
    static lp_desktop d;
    lp_desktop_init(&d, LP_RECT(0, 0, 1280, 800), NULL);
    lp_test_loop_install(&d);
    char dir[64], path[128];
    snprintf(dir, sizeof dir, "/tmp/lp-clip-XXXXXX");
    LP_ASSERT(mkdtemp(dir) != NULL);
    snprintf(path, sizeof path, "%s/port", dir);

    lp_clipboard_port p;
    lp_clipboard_port_init(&p);
    LP_ASSERT_EQ(lp_clipboard_port_open(&p, &d, path), -ENOENT);        /* not in a VM */
    LP_ASSERT_EQ(mkfifo(path, 0600), 0);
    LP_ASSERT_EQ(lp_clipboard_port_open(&p, &d, path), 0);
    int host = open(path, O_WRONLY | O_NONBLOCK);
    LP_ASSERT(host >= 0);
    lp_text_clipboard_set(lp_text_clipboard_shared(), "before", 6);
    char bytes[64];
    size_t n = message(bytes, "pasted from the host");
    LP_ASSERT_EQ(write(host, bytes, 3), 3);                             /* in two pieces */
    lp_test_loop_run(100, got_it);
    LP_ASSERT(clipboard_is("before"));
    LP_ASSERT_EQ(write(host, bytes + 3, n - 3), (ssize_t)(n - 3));
    lp_test_loop_run(2000, got_it);
    LP_ASSERT(got_it());

    /* The host's side goes away mid-message. A byte first: macOS's poll never reports a FIFO's writer
     * closing, but both systems wake for the byte, and the read after it finds the end. */
    LP_ASSERT_EQ(write(host, bytes, 1), 1);
    close(host);
    lp_test_loop_run(300, NULL);
    LP_ASSERT_EQ(p.fd, -1);
    LP_ASSERT(p.retry != NULL);
    LP_ASSERT_EQ(p.header_len, 0);                                      /* the half message is dropped */
    LP_ASSERT(got_it());

    lp_clipboard_port_close(&p);
    LP_ASSERT(p.retry == NULL && p.source == NULL);
    unlink(path);
    rmdir(dir);
    LP_ASSERT_EQ(lp_clipboard_port_open(&p, NULL, path), -ENOSYS);
}

int main(void) {
    LP_RUN(a_message_split_anywhere_becomes_the_clipboard);
    LP_RUN(an_oversized_message_is_skipped_and_an_empty_one_ignored);
    LP_RUN(the_port_is_read_on_the_event_loop_and_looked_for_again);
    LP_TEST_MAIN_END();
}
