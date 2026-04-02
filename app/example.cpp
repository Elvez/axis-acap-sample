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
#include <stdio.h>
#include <unistd.h>

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

        }
        g_list_free(discovered_streams);
    } else {
        syslog(LOG_INFO, "No VDO streams discovered from daemon");
    }

    // Try creating an attached stream for each known VDO format and
    // pick the first format that the device accepts. This helps to
    // discover which formats/subformats are supported by the camera.
    struct FormatOption { const char* name; VdoFormat fmt; } options[] = {
        {"VDO_FORMAT_NONE", VDO_FORMAT_NONE},
        {"VDO_FORMAT_H264", VDO_FORMAT_H264},
        {"VDO_FORMAT_H265", VDO_FORMAT_H265},
        {"VDO_FORMAT_JPEG", VDO_FORMAT_JPEG},
        {"VDO_FORMAT_YUV", VDO_FORMAT_YUV},
        {"VDO_FORMAT_BAYER", VDO_FORMAT_BAYER},
        {"VDO_FORMAT_IVS", VDO_FORMAT_IVS},
        {"VDO_FORMAT_RAW", VDO_FORMAT_RAW},
        {"VDO_FORMAT_RGBA", VDO_FORMAT_RGBA},
    };

    VdoStream* stream = NULL;
    bool attached = false;
    for (size_t oi = 0; oi < G_N_ELEMENTS(options); ++oi) {
        VdoMap* tryMap = vdo_map_new();
        vdo_map_set_uint32(tryMap, "channel", 0);
        vdo_map_set_uint32(tryMap, "width", 640);
        vdo_map_set_uint32(tryMap, "height", 480);
        vdo_map_set_uint32(tryMap, "buffer.strategy", VDO_BUFFER_STRATEGY_EXPLICIT);
        vdo_map_set_uint32(tryMap, "format", options[oi].fmt);

        GError* fmt_err = NULL;
        VdoStream* s = vdo_stream_new(tryMap, NULL, &fmt_err);
        if (s) {
            syslog(LOG_INFO, "Created attached stream with format %s",
                   options[oi].name);
            stream = s;
            attached = true;
            g_object_unref(tryMap);
            break;
        } else {
            syslog(LOG_INFO, "Format %s rejected: %s",
                   options[oi].name,
                   fmt_err ? fmt_err->message : "unknown");
            g_clear_error(&fmt_err);
            g_object_unref(tryMap);
        }
    }

    if (!stream) {
        syslog(LOG_WARNING, "None of the tested VDO formats could create an attached stream; trying to use an existing stream from the daemon");

        GError* list_err = NULL;
        GList* existing = vdo_stream_get_all(&list_err);
        if (list_err) {
            syslog(LOG_ERR, "Failed to list existing streams: %s",
                   list_err ? list_err->message : "unknown");
            g_clear_error(&list_err);
        }

        if (existing && existing->data) {
            stream = (VdoStream*)existing->data;
            attached = false;
            syslog(LOG_INFO, "Falling back to existing stream %p", stream);
            g_list_free(existing);
        } else {
            syslog(LOG_ERR, "No existing streams available to fall back to");
            if (existing) g_list_free(existing);
            return EXIT_FAILURE;
        }
    }

    if (attached) {
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

        syslog(LOG_INFO, "Attached stream started successfully");
    } else {
        syslog(LOG_INFO, "Using existing device-owned stream; will not allocate buffers or call start()");
    }

    // Fetch a few frames
    if (!attached) {
        // Try to attach to the datapath of the existing stream so we can
        // receive buffers. Use a short socket timeout to avoid blocking.
        GError* attach_err = NULL;
        VdoMap* intent = vdo_map_new();
        vdo_map_set_uint32(intent, "socket.timeout_ms", 1000);
        if (vdo_stream_attach(stream, intent, &attach_err)) {
            syslog(LOG_INFO, "Successfully attached to existing stream datapath");
            attached = true;
        } else {
            syslog(LOG_WARNING, "Failed to attach to stream datapath: %s",
                   attach_err ? attach_err->message : "unknown");
            g_clear_error(&attach_err);

            // Try a snapshot fallback (one-off frame) before giving up.
            VdoMap* snapMap = vdo_map_new();
            vdo_map_set_uint32(snapMap, "channel", 0);
            GError* snap_err = NULL;
            VdoBuffer* snapBuf = vdo_stream_snapshot(snapMap, &snap_err);
            if (snapBuf) {
                syslog(LOG_INFO, "Received one snapshot frame as fallback");
                // Attempt to release snapshot buffer. Best-effort only.
                vdo_stream_buffer_unref(stream, &snapBuf, NULL);
                g_object_unref(snapMap);
                g_object_unref(intent);
                // We got a frame; exit successfully.
                return EXIT_SUCCESS;
            } else {
                syslog(LOG_ERR, "Snapshot fallback failed: %s",
                       snap_err ? snap_err->message : "unknown");
                g_clear_error(&snap_err);
                g_object_unref(snapMap);
                g_object_unref(intent);
                return EXIT_FAILURE;
            }
        }
        g_object_unref(intent);
    }

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