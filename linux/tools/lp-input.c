/* lp-input: drive the desktop from a script through two uinput devices (an
 * absolute-position mouse like a VM's tablet, and a keyboard). For tests and
 * for recording parity walkthroughs in the VM. Needs /dev/uinput (root or
 * the uinput group).
 *
 *   lp-input WxH cmd...    where cmd is one of
 *     move X Y             warp the pointer to X,Y (screen pixels)
 *     click [X Y]          left click (after moving)
 *     dblclick [X Y]
 *     rclick [X Y]
 *     down | up            hold / release the left button
 *     drag X1 Y1 X2 Y2     press at 1, move in 12 steps, release at 2
 *     key NAME[+NAME...]   press and release, e.g. key super+w, key escape, key ctrl+grave
 *     keydown NAME | keyup NAME   hold / release one key (for key-repeat tests)
 *     type TEXT            ASCII text (letters, digits, space, punctuation)
 *     sleep MS
 *     mark TEXT           print TEXT (a marker for whoever watches the console)
 */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/input.h>
#include <linux/uinput.h>

static int mouse_fd = -1, kbd_fd = -1;
static int screen_w = 1280, screen_h = 800;

static void emit(int fd, int type, int code, int value) {
    struct input_event ev = { 0 };
    ev.type = (unsigned short)type;
    ev.code = (unsigned short)code;
    ev.value = value;
    if (write(fd, &ev, sizeof ev) != (ssize_t)sizeof ev) perror("write");
}
static void sync_(int fd) { emit(fd, EV_SYN, SYN_REPORT, 0); usleep(8000); }

static int create_device(const char *name, int is_mouse) {
    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (fd < 0) { perror("/dev/uinput"); exit(1); }
    struct uinput_setup setup = { 0 };
    snprintf(setup.name, sizeof setup.name, "%s", name);
    setup.id.bustype = BUS_VIRTUAL;
    setup.id.vendor = 0x1d6b;
    setup.id.product = is_mouse ? 0x0101 : 0x0102;
    ioctl(fd, UI_SET_EVBIT, EV_KEY);
    ioctl(fd, UI_SET_EVBIT, EV_SYN);
    if (is_mouse) {
        ioctl(fd, UI_SET_EVBIT, EV_ABS);
        ioctl(fd, UI_SET_KEYBIT, BTN_LEFT);
        ioctl(fd, UI_SET_KEYBIT, BTN_RIGHT);
        ioctl(fd, UI_SET_KEYBIT, BTN_MIDDLE);
        struct uinput_abs_setup ax = { .code = ABS_X, .absinfo = { .minimum = 0, .maximum = screen_w - 1 } };
        struct uinput_abs_setup ay = { .code = ABS_Y, .absinfo = { .minimum = 0, .maximum = screen_h - 1 } };
        ioctl(fd, UI_ABS_SETUP, &ax);
        ioctl(fd, UI_ABS_SETUP, &ay);
    } else {
        for (int k = 1; k < 248; k++) ioctl(fd, UI_SET_KEYBIT, k);
    }
    if (ioctl(fd, UI_DEV_SETUP, &setup) < 0 || ioctl(fd, UI_DEV_CREATE) < 0) { perror("uinput setup"); exit(1); }
    usleep(400000); /* let udev/libinput pick it up */
    return fd;
}

static void move(int x, int y) { emit(mouse_fd, EV_ABS, ABS_X, x); emit(mouse_fd, EV_ABS, ABS_Y, y); sync_(mouse_fd); }
static void button(int code, int value) { emit(mouse_fd, EV_KEY, code, value); sync_(mouse_fd); }

struct keyname { const char *name; int code; };
static const struct keyname KEYS[] = {
    { "escape", KEY_ESC }, { "esc", KEY_ESC }, { "enter", KEY_ENTER }, { "return", KEY_ENTER }, { "space", KEY_SPACE }, { "tab", KEY_TAB },
    { "backspace", KEY_BACKSPACE }, { "up", KEY_UP }, { "down", KEY_DOWN }, { "left", KEY_LEFT }, { "right", KEY_RIGHT },
    { "super", KEY_LEFTMETA }, { "meta", KEY_LEFTMETA }, { "cmd", KEY_LEFTMETA }, { "ctrl", KEY_LEFTCTRL }, { "alt", KEY_LEFTALT }, { "shift", KEY_LEFTSHIFT },
    { "grave", KEY_GRAVE }, { "minus", KEY_MINUS }, { "equal", KEY_EQUAL }, { "comma", KEY_COMMA }, { "dot", KEY_DOT }, { "period", KEY_DOT }, { "slash", KEY_SLASH },
    { "bracketleft", KEY_LEFTBRACE }, { "bracketright", KEY_RIGHTBRACE }, { "semicolon", KEY_SEMICOLON }, { "apostrophe", KEY_APOSTROPHE }, { "backslash", KEY_BACKSLASH },
    { "delete", KEY_DELETE }, { "home", KEY_HOME }, { "end", KEY_END }, { "pageup", KEY_PAGEUP }, { "pagedown", KEY_PAGEDOWN },
    { "a", KEY_A }, { "b", KEY_B }, { "c", KEY_C }, { "d", KEY_D }, { "e", KEY_E }, { "f", KEY_F }, { "g", KEY_G }, { "h", KEY_H }, { "i", KEY_I },
    { "j", KEY_J }, { "k", KEY_K }, { "l", KEY_L }, { "m", KEY_M }, { "n", KEY_N }, { "o", KEY_O }, { "p", KEY_P }, { "q", KEY_Q }, { "r", KEY_R },
    { "s", KEY_S }, { "t", KEY_T }, { "u", KEY_U }, { "v", KEY_V }, { "w", KEY_W }, { "x", KEY_X }, { "y", KEY_Y }, { "z", KEY_Z },
    { "0", KEY_0 }, { "1", KEY_1 }, { "2", KEY_2 }, { "3", KEY_3 }, { "4", KEY_4 }, { "5", KEY_5 }, { "6", KEY_6 }, { "7", KEY_7 }, { "8", KEY_8 }, { "9", KEY_9 },
    { "f1", KEY_F1 }, { "f2", KEY_F2 }, { "f3", KEY_F3 }, { "f4", KEY_F4 }, { "f5", KEY_F5 }, { "f6", KEY_F6 },
};

static int keycode(const char *name) {
    for (size_t i = 0; i < sizeof KEYS / sizeof KEYS[0]; i++) if (strcmp(KEYS[i].name, name) == 0) return KEYS[i].code;
    return -1;
}

static void chord(const char *spec) {
    char buf[128];
    snprintf(buf, sizeof buf, "%s", spec);
    int codes[8], n = 0;
    for (char *tok = strtok(buf, "+"); tok && n < 8; tok = strtok(NULL, "+")) {
        int c = keycode(tok);
        if (c < 0) { fprintf(stderr, "lp-input: unknown key %s\n", tok); return; }
        codes[n++] = c;
    }
    for (int i = 0; i < n; i++) { emit(kbd_fd, EV_KEY, codes[i], 1); sync_(kbd_fd); }
    for (int i = n - 1; i >= 0; i--) { emit(kbd_fd, EV_KEY, codes[i], 0); sync_(kbd_fd); }
    usleep(40000);
}

static void type_text(const char *text) {
    for (; *text; text++) {
        char ch = *text;
        int shift = 0, code = -1;
        char lower[2] = { ch, 0 };
        if (ch >= 'A' && ch <= 'Z') { shift = 1; lower[0] = (char)(ch + 32); }
        if (ch == ' ') code = KEY_SPACE;
        else if (ch == '\n') code = KEY_ENTER;
        else if (ch == '-') code = KEY_MINUS;
        else if (ch == '.') code = KEY_DOT;
        else if (ch == '/') code = KEY_SLASH;
        else if (ch == ',') code = KEY_COMMA;
        else if (ch == '=') code = KEY_EQUAL;
        else code = keycode(lower);
        if (code < 0) continue;
        if (shift) { emit(kbd_fd, EV_KEY, KEY_LEFTSHIFT, 1); sync_(kbd_fd); }
        emit(kbd_fd, EV_KEY, code, 1); sync_(kbd_fd);
        emit(kbd_fd, EV_KEY, code, 0); sync_(kbd_fd);
        if (shift) { emit(kbd_fd, EV_KEY, KEY_LEFTSHIFT, 0); sync_(kbd_fd); }
        usleep(30000);
    }
}

int main(int argc, char **argv) {
    if (argc < 3 || sscanf(argv[1], "%dx%d", &screen_w, &screen_h) != 2) {
        fprintf(stderr, "usage: lp-input WxH cmd...  (see the source for commands)\n");
        return 2;
    }
    mouse_fd = create_device("Liquid Platinum test pointer", 1);
    kbd_fd = create_device("Liquid Platinum test keyboard", 0);
    for (int i = 2; i < argc; i++) {
        const char *cmd = argv[i];
        if (strcmp(cmd, "move") == 0 && i + 2 < argc) { move(atoi(argv[i + 1]), atoi(argv[i + 2])); i += 2; }
        else if ((strcmp(cmd, "click") == 0 || strcmp(cmd, "dblclick") == 0 || strcmp(cmd, "rclick") == 0)) {
            if (i + 2 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9') { move(atoi(argv[i + 1]), atoi(argv[i + 2])); i += 2; usleep(60000); }
            int btn = strcmp(cmd, "rclick") == 0 ? BTN_RIGHT : BTN_LEFT;
            button(btn, 1); usleep(40000); button(btn, 0);
            if (strcmp(cmd, "dblclick") == 0) { usleep(90000); button(btn, 1); usleep(40000); button(btn, 0); }
            usleep(60000);
        }
        else if (strcmp(cmd, "down") == 0) button(BTN_LEFT, 1);
        else if (strcmp(cmd, "up") == 0) button(BTN_LEFT, 0);
        else if (strcmp(cmd, "drag") == 0 && i + 4 < argc) {
            int x1 = atoi(argv[i + 1]), y1 = atoi(argv[i + 2]), x2 = atoi(argv[i + 3]), y2 = atoi(argv[i + 4]);
            move(x1, y1); usleep(60000); button(BTN_LEFT, 1); usleep(60000);
            for (int s = 1; s <= 12; s++) { move(x1 + (x2 - x1) * s / 12, y1 + (y2 - y1) * s / 12); usleep(16000); }
            usleep(60000); button(BTN_LEFT, 0);
            i += 4;
        }
        else if (strcmp(cmd, "key") == 0 && i + 1 < argc) { chord(argv[i + 1]); i++; }
        else if ((strcmp(cmd, "keydown") == 0 || strcmp(cmd, "keyup") == 0) && i + 1 < argc) {
            int code = keycode(argv[i + 1]);
            if (code < 0) { fprintf(stderr, "lp-input: unknown key %s\n", argv[i + 1]); return 2; }
            emit(kbd_fd, EV_KEY, code, strcmp(cmd, "keydown") == 0 ? 1 : 0); sync_(kbd_fd);
            i++;
        }
        else if (strcmp(cmd, "type") == 0 && i + 1 < argc) { type_text(argv[i + 1]); i++; }
        else if (strcmp(cmd, "sleep") == 0 && i + 1 < argc) { usleep((useconds_t)atoi(argv[i + 1]) * 1000); i++; }
        else if (strcmp(cmd, "mark") == 0 && i + 1 < argc) { printf("%s\n", argv[i + 1]); fflush(stdout); i++; }
        else { fprintf(stderr, "lp-input: bad command at %s\n", cmd); return 2; }
    }
    usleep(200000);
    ioctl(mouse_fd, UI_DEV_DESTROY);
    ioctl(kbd_fd, UI_DEV_DESTROY);
    return 0;
}
