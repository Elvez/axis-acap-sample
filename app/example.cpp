/**
 * Minimal VDO stream example (Axis OS 9.x legacy compatible)
 */

#include <vdo-stream.h>
#include <vdo-map.h>
#include <vdo-buffer.h>

#include <syslog.h>
#include <stdlib.h>

int main() {
    openlog("opencv_app", LOG_PID | LOG_CONS, LOG_USER);

    GError* error = NULL;
    auto streams = vdo_stream_get_all(&error);
    if (error)
    {
      syslog(LOG_ERR, "Failed to get all streams: %s",
               error ? error->message : "unknown");
    }
    syslog(LOG_INFO, "Got streams : %d", g_list_length(streams));

    // 3. Start stream
    if (!vdo_stream_start(g_list_first(streams)->data, &error)) {
        syslog(LOG_ERR, "Failed to start stream: %s",
               error ? error->message : "unknown");
        g_clear_error(&error);
        return EXIT_FAILURE;
    }

    syslog(LOG_INFO, "Stream started successfully");

    // 4. Fetch frames
    for (int i = 0; i < 10; i++) {

        VdoBuffer* buffer = vdo_stream_get_buffer(g_list_first(streams)->data, &error);

        if (!buffer) {
            syslog(LOG_ERR, "Failed to get buffer: %s",
                   error ? error->message : "unknown");
            g_clear_error(&error);
            break;
        }

        // 👉 No metadata APIs available here in legacy VDO
        syslog(LOG_INFO, "Frame %d received", i);

        // release buffer back to VDO
        if (!vdo_stream_buffer_unref(g_list_first(streams)->data, &buffer, &error)) {
            syslog(LOG_ERR, "Failed to release buffer: %s",
                   error ? error->message : "unknown");
            g_clear_error(&error);
            break;
        }
    }

    syslog(LOG_INFO, "Done");

    return EXIT_SUCCESS;
}