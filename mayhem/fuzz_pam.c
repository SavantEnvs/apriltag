// Fuzz the PAM (P7) reader and its RGBA conversion.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "common/image_u8x4.h"
#include "common/pam.h"

int LLVMFuzzerInitialize(int *argc, char ***argv)
{
    (void) argc;
    (void) argv;
    // the reader prints diagnostics to stdout on malformed headers
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
    const char *tmpdir = getenv("TMPDIR");
    char path[4096];
    snprintf(path, sizeof(path), "%s/fuzz_pam_XXXXXX", tmpdir ? tmpdir : "/tmp");
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

    pam_t *pam = pam_create_from_file(path);
    if (pam)
        pam_destroy(pam);
    image_u8x4_t *im = image_u8x4_create_from_pam(path);
    if (im)
        image_u8x4_destroy(im);

    unlink(path);
    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    int rc = run_one(data, size);
    reset_stdout();
    return rc;
}
