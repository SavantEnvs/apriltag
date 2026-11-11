// Fuzz the PNM (P4/P5/P6) readers and the grayscale/RGB/RGBA conversions.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "common/image_u8.h"
#include "common/image_u8x3.h"
#include "common/image_u8x4.h"

int LLVMFuzzerInitialize(int *argc, char ***argv)
{
    (void) argc;
    (void) argv;
    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    const char *tmpdir = getenv("TMPDIR");
    char path[4096];
    snprintf(path, sizeof(path), "%s/fuzz_pnm_XXXXXX", tmpdir ? tmpdir : "/tmp");
    int fd = mkstemp(path);
    if (fd < 0)
        return 0;
    size_t off = 0;
    while (off < size) {
        ssize_t n = write(fd, data + off, size - off);
        if (n <= 0)
            break;
        off += (size_t) n;
    }
    close(fd);

    image_u8_t *im = image_u8_create_from_pnm(path);
    if (im)
        image_u8_destroy(im);
    image_u8x3_t *im3 = image_u8x3_create_from_pnm(path);
    if (im3)
        image_u8x3_destroy(im3);
    image_u8x4_t *im4 = image_u8x4_create_from_pnm(path);
    if (im4)
        image_u8x4_destroy(im4);

    unlink(path);
    return 0;
}
