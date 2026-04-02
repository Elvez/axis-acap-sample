// /**
//  * Copyright (C) 2020 Axis Communications AB, Lund, Sweden
//  *
//  * Licensed under the Apache License, Version 2.0 (the "License");
//  * you may not use this file except in compliance with the License.
//  * You may obtain a copy of the License at
//  *
//  *     http://www.apache.org/licenses/LICENSE-2.0
//  *
//  * Unless required by applicable law or agreed to in writing, software
//  * distributed under the License is distributed on an "AS IS" BASIS,
//  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  * See the License for the specific language governing permissions and
//  * limitations under the License.
//  */

// #include <stdlib.h>
// #include <syslog.h>
// #include <opencv2/imgproc.hpp>
// #include <opencv2/video.hpp>

// #include "imgprovider.h"

// using namespace cv;

// int main(int argc, char* argv[]) {
//   openlog("opencv_app", LOG_PID|LOG_CONS, LOG_USER);
//   syslog(LOG_INFO, "Running OpenCV example with VDO as video source");
//   ImgProvider_t* provider = NULL;

//   // The desired width and height of the BGR frame
//   unsigned int width = 1024;
//   unsigned int height = 576;

//   // // chooseStreamResolution gets the least resource intensive stream
//   // // that exceeds or equals the desired resolution specified above
//   // unsigned int streamWidth = 0;
//   // unsigned int streamHeight = 0;
//   // if (!chooseStreamResolution(width, height, &streamWidth,
//   //                             &streamHeight)) {
//   //     syslog(LOG_ERR, "%s: Failed choosing stream resolution", __func__);
//   //     exit(1);
//   // }

//   syslog(LOG_INFO, "Creating VDO image provider and creating stream %d x %d",
//           width, height);
//   provider = createImgProvider(width, height, 2, VDO_FORMAT_RGBA);
//   if (!provider) {
//     syslog(LOG_ERR, "%s: Failed to create ImgProvider", __func__);
//     exit(2);
//   }

//   syslog(LOG_INFO, "Start fetching video frames from VDO");
//   if (!startFrameFetch(provider)) {
//     syslog(LOG_ERR, "%s: Failed to fetch frames from VDO", __func__);
//     exit(3);
//   }

//   // Create the background subtractor
//   Ptr<BackgroundSubtractorMOG2> bgsub = createBackgroundSubtractorMOG2();

//   // Create the filtering element. Its size influences what is considered
//   // noise, with a bigger size corresponding to more denoising
//   Mat kernel = getStructuringElement(MORPH_ELLIPSE, Size(9, 9));

//   // Create OpenCV Mats for the camera frame (nv12), the converted frame (bgr)
//   // and the foreground frame that is outputted by the background subtractor
//   Mat bgra_mat = Mat(height, width, CV_8UC4);
//   // Mat nv12_mat = Mat(height * 3 / 2, width, CV_8UC1);
//   Mat fg;

//   while (true) {
//     // Get the latest NV12 image frame from VDO using the imageprovider
//     VdoBuffer* buf = getLastFrameBlocking(provider);
//     if (!buf) {
//       syslog(LOG_INFO, "No more frames available, exiting");
//       exit(0);
//     }

//     // Assign the VDO image buffer to the nv12_mat OpenCV Mat.
//     // This specific Mat is used as it is the one we created for NV12,
//     // which has a different layout than e.g., BGR.
//     bgra_mat.data = static_cast<uint8_t*>(vdo_buffer_get_data(buf));

//     // Convert the NV12 data to BGR
//     // cvtColor(bgra_mat, bgra_mat, COLOR_, 3);

//     // Perform background subtraction on the bgr image with
//     // learning rate 0.005. The resulting image should have
//     // pixel intensities > 0 only where changes have occurred
//     bgsub->apply(bgra_mat, fg, 0.005);

//     // Filter noise from the image with the filtering element
//     morphologyEx(fg, fg, MORPH_OPEN, kernel);

//     // We define movement in the image as any pixel being non-zero
//     int nonzero_pixels = countNonZero(fg);
//     if (nonzero_pixels > 0) {
//       syslog(LOG_INFO, "Motion detected: YES");
//     } else {
//       syslog(LOG_INFO, "Motion detected: NO");
//     }

//     // Release the VDO frame buffer
//     returnFrame(provider, buf);
//   }
//   return EXIT_SUCCESS;
// }

/**
 * Minimal VDO stream example (ACAP / Axis OS 9.x compatible)
 */

#include <vdo-stream.h>
#include <vdo-map.h>
#include <vdo-buffer.h>
#include <vdo-frame.h>

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