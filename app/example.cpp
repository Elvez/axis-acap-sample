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

    // Enumerate and dump any streams provided by the running VDO daemon.
    GList* discovered_streams = vdo_stream_get_all(&error);
    if (error) {
        syslog(LOG_ERR, "Failed to enumerate VDO streams: %s",
               error ? error->message : "unknown");
        g_clear_error(&error);
    } else if (discovered_streams) {
        syslog(LOG_INFO, "Discovered %d VDO streams", g_list_length(discovered_streams));
        for (GList* l = discovered_streams; l; l = l->next) {
            VdoStream* s = (VdoStream*)l->data;
            syslog(LOG_INFO, "Dumping stream object %p", s);

            // Obtain stream info/settings as a VdoMap and dump it. Use
            // `vdo_stream_get_info()` which is available in this SDK.
            GError* map_err = NULL;
            VdoMap* map = vdo_stream_get_info(s, &map_err);
            if (map) {
                vdo_map_dump(map);
                g_object_unref(map);
            } else {
                syslog(LOG_INFO, "  Could not get stream info: %s",
                       map_err ? map_err->message : "unknown");
                g_clear_error(&map_err);
            }
        }
        g_list_free(discovered_streams);
    } else {
        syslog(LOG_INFO, "No VDO streams discovered from daemon");
    }

    // Create an in-process stream attached to the VDO control socket so
    // we can allocate and enqueue buffers explicitly. This avoids the
    // "Not attached to control socket" error when calling
    // `vdo_stream_buffer_alloc()` on externally-owned streams.
    VdoMap* vdoMap = vdo_map_new();
    vdo_map_set_uint32(vdoMap, "channel", 0);
    vdo_map_set_uint32(vdoMap, "width", 640);
    vdo_map_set_uint32(vdoMap, "height", 480);
    vdo_map_set_uint32(vdoMap, "buffer.strategy", VDO_BUFFER_STRATEGY_EXPLICIT);

    VdoStream* stream = vdo_stream_new(vdoMap, NULL, &error);
    if (!stream) {
        syslog(LOG_ERR, "Failed to create attached vdo stream: %s",
               error ? error->message : "unknown");
        g_clear_error(&error);
        g_object_unref(vdoMap);
        return EXIT_FAILURE;
    }
    g_object_unref(vdoMap);
    syslog(LOG_INFO, "Created ced VDO stream successfully");

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