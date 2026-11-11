// Fuzz the baseline JPEG decoder and its image conversions.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common/image_u8.h"
#include "common/image_u8x3.h"
#include "common/pjpeg.h"

int LLVMFuzzerInitialize(int *argc, char ***argv)
{
    (void) argc;
    (void) argv;
    // the decoder prints diagnostics to stdout on corrupt streams
    // (a tmpfile, truncated after every input, keeps the run free of absolute write paths)
    FILE *sink = tmpfile();
    if (!sink || dup2(fileno(sink), STDOUT_FILENO) < 0)
        return 1;
    return 0;
}

static void reset_stdout(void)
{
    fflush(stdout);
    if (ftruncate(STDOUT_FILENO, 0) == 0)
        lseek(STDOUT_FILENO, 0, SEEK_SET);
}

static int run_one(const uint8_t *data, size_t size)
{
    if (size < 2 || size > (1u << 24))
        return -1;

    const uint32_t flags = data[0] & (PJPEG_STRICT | PJPEG_MJPEG);
    uint8_t *buf = malloc(size - 1);
    memcpy(buf, data + 1, size - 1);

    int error = 0;
    pjpeg_t *pj = pjpeg_create_from_buffer(buf, (int) (size - 1), flags, &error);
    if (pj) {
        if (!pj->error) {
            // pjpeg.c asserts these preconditions (compiled out in the Release library), so honor them here
            const pjpeg_component_t *c0 = pj->ncomponents > 0 ? &pj->components[0] : NULL;
            if (c0 && c0->width >= pj->width && c0->height >= pj->height) {
                image_u8_t *im = pjpeg_to_u8_baseline(pj);
                if (im)
                    image_u8_destroy(im);
            }
            if (pj->ncomponents == 3) {
                image_u8x3_t *im3 = pjpeg_to_u8x3_baseline(pj);
                if (im3)
                    image_u8x3_destroy(im3);
            }
        }
        pjpeg_destroy(pj);
    }
    free(buf);
    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    int rc = run_one(data, size);
    reset_stdout();
    return rc;
}
