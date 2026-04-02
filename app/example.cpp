#include <vdo-error.h>
#include <vdo-stream.h>
#include <vdo-channel.h>
 
#include <cerrno>
#include <cstdint>
#include <cstdlib>
 
#include <poll.h>
#include <unistd.h>
 
#include <print>
#include <chrono>
 
static bool
example_detect_sensors(GError** error)
{
    g_autoptr(VdoMap) info = vdo_channel_get_info(VDO_CHANNEL_GLOBAL, error);
    if (!info)
        return false;
 
    auto sensor_count = vdo_map_get_uint32(info, "input.count", 0);
    std::println("<6>Sensors: {}", sensor_count);
 
    return true;
}
 
static bool
example_detect_resolution(VdoStream* stream, GError** error)
{
    g_autoptr(VdoMap) info = vdo_stream_get_info(stream, error);
    if (!info)
        return false;
 
    // 'info' includes global-rotation.
    auto w = vdo_map_get_uint32(info, "width",  0u);
    auto h = vdo_map_get_uint32(info, "height", 0u);
    std::println("<6>Resolution: {}x{}", w, h);
 
    return true;
}
 
int main()
{
    g_autoptr(GError) error = nullptr;
    auto failed = [&error] {
        // Maintenance/Installation in progress (e.g. Global-Rotation)
        if (vdo_error_is_expected(&error)) {
            std::println(stderr, "<6>info: {}",  error->message);
            return EXIT_SUCCESS;
        } else {
            std::println(stderr, "<3>error: {}",  error->message);
            return EXIT_FAILURE;
        }
    };
 
    if (!example_detect_sensors(&error))
        return failed();
 
    g_autoptr(VdoMap) settings = vdo_map_new();
 
    // Stream from the first channel and use its default settings.
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
 
    pollfd fds = {.fd = fd, .events = POLL_IN,};
 
    if (!vdo_stream_start(stream, &error))
        return failed();
 
    // Fetch 10 frames
    for (size_t i = 0zu; i < 10zu;) {
        int status = TEMP_FAILURE_RETRY(poll(&fds, 1, -1));
        if (status < 0)
            return failed();
 
        g_autoptr(VdoBuffer) buffer = vdo_stream_get_buffer(stream, &error);
        if (!buffer && g_error_matches(error, VDO_ERROR, VDO_ERROR_NO_DATA))
            continue; // Transient Error -> Retry!
 
        if (!buffer)
            return failed();
 
        VdoFrame* frame = vdo_buffer_get_frame(buffer);
 
        // Low jitter monotonic capture timestamp. (See g_get_monotonic_time())
        auto pts    = vdo_frame_get_timestamp(frame);
        auto pts_us = std::chrono::microseconds(pts);
        std::println("{:%T}", pts_us);
 
        // Allow VDO to reuse the frame/buffer.
        if (!vdo_stream_buffer_unref(stream, &buffer, &error))
            return failed();
 
        // Only successful frames count!
        i += 1;
    }
 
    return EXIT_SUCCESS;
}