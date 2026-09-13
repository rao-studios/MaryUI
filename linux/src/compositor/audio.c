/* PipeWire's speakers and microphones on the scene (PARITY D23): lp_audio rides the compositor's event loop,
 * and what it learns repaints the windows that show it (System Settings › Sound). */
#include <errno.h>
#include <wlr/util/log.h>

#include "maryui/lp_audio.h"
#include "server.h"

void mui_audio_init(struct mui_server *server) {
    if (lp_audio_start(&server->desktop.audio) == -ENOSYS) wlr_log(WLR_INFO, "audio: built without PipeWire; Settings shows no devices");
    else wlr_log(WLR_INFO, "audio: following PipeWire's devices");
}

void mui_audio_finish(struct mui_server *server) { lp_audio_free(&server->desktop.audio); }
