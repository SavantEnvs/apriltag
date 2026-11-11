// Fuzz the whole detection pipeline (threshold, segmentation, quad fit, decode, pose) on a raw 8-bit image.
// Input: [0]=tag family, [1]=detector option bits, [2]=width-1, [3]=quad-threshold knobs, rest=pixel rows.
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "apriltag.h"
#include "apriltag_pose.h"
#include "tag16h5.h"
#include "tag25h9.h"
#include "tag36h11.h"
#include "tagCircle21h7.h"
#include "tagStandard41h12.h"
#include "aruco/tagAruco4x4_50.h"
#include "aruco/tagArucoMIP36h12.h"

#define NFAM 7
#define MAX_ROWS 512

static apriltag_family_t *fams[NFAM];
static apriltag_detector_t *dets[NFAM];

int LLVMFuzzerInitialize(int *argc, char ***argv)
{
    (void) argc;
    (void) argv;
    fams[0] = tag36h11_create();
    fams[1] = tag16h5_create();
    fams[2] = tag25h9_create();
    fams[3] = tagStandard41h12_create();
    fams[4] = tagCircle21h7_create();
    fams[5] = tagArucoMIP36h12_create();
    fams[6] = tagAruco4x4_50_create();
    for (int i = 0; i < NFAM; i++) {
        dets[i] = apriltag_detector_create();
        apriltag_detector_add_family_bits(dets[i], fams[i], i < 5 ? 2 : 1);
    }
    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 5)
        return -1;

    apriltag_detector_t *td = dets[data[0] % NFAM];
    const uint8_t opt = data[1];
    const unsigned width = (unsigned) data[2] + 1;
    const uint8_t knobs = data[3];
    data += 4;
    size -= 4;

    unsigned height = size / width;
    if (height == 0)
        return -1;
    if (height > MAX_ROWS)
        height = MAX_ROWS;

    // every tunable is reassigned on every input, so no state leaks between inputs
    static const float decimate[4] = {1.0f, 1.5f, 2.0f, 4.0f};
    static const float sigma[4] = {0.0f, 0.5f, 0.8f, 2.0f};
    static const double sharpening[4] = {0.0, 0.25, 0.5, 1.0};
    td->nthreads = 1;
    td->debug = false;
    td->refine_edges = (opt & 1) != 0;
    td->qtp.deglitch = (opt >> 1) & 1;
    td->quad_decimate = decimate[(opt >> 2) & 3];
    td->quad_sigma = sigma[(opt >> 4) & 3];
    td->decode_sharpening = sharpening[(opt >> 6) & 3];
    td->qtp.max_nmaxima = 1 + (knobs & 15);
    td->qtp.min_white_black_diff = (knobs >> 4) * 5;
    td->qtp.min_cluster_pixels = 4 + (knobs & 3) * 10;
    td->qtp.max_line_fit_mse = 10.0f;
    td->qtp.cos_critical_rad = cosf(10.0f * (float) M_PI / 180.0f);

    image_u8_t *im = image_u8_create(width, height);
    for (unsigned y = 0; y < height; y++)
        memcpy(&im->buf[y * im->stride], data + (size_t) y * width, width);

    zarray_t *detections = apriltag_detector_detect(td, im);

    for (int i = 0; i < zarray_size(detections); i++) {
        apriltag_detection_t *det;
        zarray_get(detections, i, &det);
        apriltag_detection_info_t info = {det, 0.16, 600.0, 600.0, width / 2.0, height / 2.0};
        apriltag_pose_t pose;
        estimate_tag_pose(&info, &pose);
        matd_destroy(pose.R);
        matd_destroy(pose.t);
    }

    apriltag_detections_destroy(detections);
    image_u8_destroy(im);
    return 0;
}
