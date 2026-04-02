#include <vdo-error.h>
#include <vdo-stream.h>
#include <vdo-channel.h>

#include <cerrno>
#include <cstdint>
#include <cstdlib>

#include <poll.h>
#include <unistd.h>

#include <chrono>
#include <syslog.h>

static bool
example_detect_sensors(GError** error)
{
    g_autoptr(VdoMap) info = vdo_channel_get_info(VDO_CHANNEL_GLOBAL, error);
    if (!info)
        return false;

    auto sensor_count = vdo_map_get_uint32(info, "input.count", 0);
    syslog(LOG_INFO, "Sensors: %u", sensor_count);

    return true;
}

static bool
example_detect_resolution(VdoStream* stream, GError** error)
{
    g_autoptr(VdoMap) info = vdo_stream_get_info(stream, error);
    if (!info)
        return false;

    auto w = vdo_map_get_uint32(info, "width",  0u);
    auto h = vdo_map_get_uint32(info, "height", 0u);
    syslog(LOG_INFO, "Resolution: %ux%u", w, h);

    return true;
}

int main()
{
    openlog("vdo_example", LOG_PID | LOG_CONS, LOG_USER);

    g_autoptr(GError) error = nullptr;

    auto failed = [&error] {
        if (vdo_error_is_expected(&error)) {
            syslog(LOG_INFO, "info: %s", error->message);
            return EXIT_SUCCESS;
        } else {
            syslog(LOG_ERR, "error: %s", error->message);
            return EXIT_FAILURE;
        }
    };

    if (!example_detect_sensors(&error))
        return failed();

    g_autoptr(VdoMap) settings = vdo_map_new();

    vdo_map_set_uint32(settings, "channel", 1u);
    vdo_map_set_uint32(settings, "format", VDO_FORMAT_H264);
    vdo_map_set_boolean(settings, "socket.blocking", false);

    g_autoptr(VdoStream) stream = vdo_stream_new(settings, nullptr, &error);
    if (!stream)
        return failed();

    if (!example_detect_resolution(stream, &error))
        return failed();

    int fd = vdo_stream_get_fd(stream, &error);
    if (fd < 0)
        return failed();

    pollfd fds = {.fd = fd, .events = POLL_IN};

    if (!vdo_stream_start(stream, &error))
        return failed();

    for (size_t i = 0zu; i < 10zu;) {
        int status = TEMP_FAILURE_RETRY(poll(&fds, 1, -1));
        if (status < 0)
            return failed();

        g_autoptr(VdoBuffer) buffer = vdo_stream_get_buffer(stream, &error);
        if (!buffer && g_error_matches(error, VDO_ERROR, VDO_ERROR_NO_DATA))
            continue;

        if (!buffer)
            return failed();

        VdoFrame* frame = vdo_buffer_get_frame(buffer);

        auto pts    = vdo_frame_get_timestamp(frame);
        auto pts_us = std::chrono::microseconds(pts);

        syslog(LOG_INFO, "PTS (us): %lld", (long long)pts_us.count());

        if (!vdo_stream_buffer_unref(stream, &buffer, &error))
            return failed();

        i += 1;
    }

    return EXIT_SUCCESS;
}