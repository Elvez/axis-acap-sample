/**
 * Minimal VDO stream example (Axis ACAP SDK 3.1 compatible)
 *
 * This example allocates and enqueues a small set of VDO buffers before
 * starting the stream. Without any buffers queued the VDO implementation
 * may block in `vdo_stream_start()` waiting for buffers and cause the
 * application to appear to hang.
 */

#include <vdo-stream.h>
#include <vdo-map.h>
#include <vdo-buffer.h>

#include <glib.h>
#include <syslog.h>
#include <stdlib.h>

#define NUM_VDO_BUFFERS 3

int main() {
    openlog("opencv_app", LOG_PID | LOG_CONS, LOG_USER);

    GError* error = NULL;

    GList* streams = vdo_stream_get_all(&error);
    if (error) {
        syslog(LOG_ERR, "Failed to get all streams: %s",
               error ? error->message : "unknown");
        g_clear_error(&error);
        return EXIT_FAILURE;
    }
    if (!streams) {
        syslog(LOG_ERR, "No VDO streams available");
        return EXIT_FAILURE;
    }

    VdoStream* stream = (VdoStream*)g_list_first(streams)->data;
    syslog(LOG_INFO, "Got streams : %d", g_list_length(streams));

    // Allocate and enqueue a few buffers BEFORE starting the stream.
    VdoBuffer* bufs[NUM_VDO_BUFFERS] = {0};
    for (int i = 0; i < NUM_VDO_BUFFERS; ++i) {
        bufs[i] = vdo_stream_buffer_alloc(stream, NULL, &error);
        if (!bufs[i]) {
            syslog(LOG_ERR, "Failed to allocate vdo buffer: %s",
                   error ? error->message : "unknown");
            g_clear_error(&error);
            // cleanup already allocated buffers
            for (int j = 0; j < i; ++j) {
                vdo_stream_buffer_unref(stream, &bufs[j], NULL);
            }
            return EXIT_FAILURE;
        }

        if (!vdo_stream_buffer_enqueue(stream, bufs[i], &error)) {
            syslog(LOG_ERR, "Failed to enqueue vdo buffer: %s",
                   error ? error->message : "unknown");
            g_clear_error(&error);
            for (int j = 0; j <= i; ++j) {
                vdo_stream_buffer_unref(stream, &bufs[j], NULL);
            }
            return EXIT_FAILURE;
        }
    }

    // Start stream
    if (!vdo_stream_start(stream, &error)) {
        syslog(LOG_ERR, "Failed to start stream: %s",
               error ? error->message : "unknown");
        g_clear_error(&error);
        return EXIT_FAILURE;
    }

    syslog(LOG_INFO, "Stream started successfully");

    // Fetch a few frames
    for (int i = 0; i < 10; i++) {
        VdoBuffer* buffer = vdo_stream_get_buffer(stream, &error);
        if (!buffer) {
            syslog(LOG_ERR, "Failed to get buffer: %s",
                   error ? error->message : "unknown");
            g_clear_error(&error);
            break;
        }

        syslog(LOG_INFO, "Frame %d received", i);

        // Return buffer back to VDO
        if (!vdo_stream_buffer_unref(stream, &buffer, &error)) {
            syslog(LOG_ERR, "Failed to release buffer: %s",
                   error ? error->message : "unknown");
            g_clear_error(&error);
            break;
        }
    }

    syslog(LOG_INFO, "Done");

    return EXIT_SUCCESS;
}