
/**
 * Minimal VDO stream example (ACAP / Axis OS 9.x compatible)
 */

#include <vdo-stream.h>
#include <vdo-map.h>
#include <vdo-buffer.h>
#include <vdo-frame.h>
#include <stdint.h>
#include <syslog.h>
#include <stdlib.h>

int main() {
    openlog("vdo_minimal", LOG_PID | LOG_CONS, LOG_USER);

    GError* error = NULL;

    // 1. Create settings
    VdoMap* settings = vdo_map_new();
    if (!settings) {
        syslog(LOG_ERR, "Failed to create settings map");
        return EXIT_FAILURE;
    }

    // ⚠️ Only set channel (DO NOT set format)
    vdo_map_set_uint32(settings, "channel", 1);

    // 2. Create stream
    VdoStream* stream = vdo_stream_new(settings, NULL, &error);
    if (!stream) {
        syslog(LOG_ERR, "Failed to create stream: %s",
               error ? error->message : "unknown");
        return EXIT_FAILURE;
    }

    // 3. Start stream
    if (!vdo_stream_start(stream, &error)) {
        syslog(LOG_ERR, "Failed to start stream: %s",
               error ? error->message : "unknown");
        return EXIT_FAILURE;
    }

    syslog(LOG_INFO, "Stream started successfully");

    // 4. Fetch frames
    for (int i = 0; i < 10; i++) {

        VdoBuffer* buffer = vdo_stream_get_buffer(stream, &error);

        if (!buffer) {
            syslog(LOG_ERR, "Failed to get buffer: %s",
                   error ? error->message : "unknown");
            break;
        }

        VdoFrame* frame = vdo_buffer_get_frame(buffer);

        uint32_t format = vdo_frame_get_format(frame);
        uint32_t width  = vdo_frame_get_width(frame);
        uint32_t height = vdo_frame_get_height(frame);

        syslog(LOG_INFO, "Frame %d: format=%u, %ux%u",
               i, format, width, height);

        // release buffer back to VDO
        if (!vdo_stream_buffer_unref(stream, &buffer, &error)) {
            syslog(LOG_ERR, "Failed to release buffer: %s",
                   error ? error->message : "unknown");
            break;
        }
    }

    syslog(LOG_INFO, "Done");

    return EXIT_SUCCESS;
}