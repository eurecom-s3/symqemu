#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv) {
    uint8_t x;
    int fd = open(argv[1], O_RDONLY);
    if (fd < 0 || read(fd, &x, 1) != 1)
        return 1;

    /* Legitimate branch: the only solution is x == 0x42. */
    if (x == 0x42)
        puts("magic");

    uint32_t v1 = (uint32_t)x | 0x100;      /* bit 8 always set -> bsr(v1) == 8 */
    uint32_t v2 = ((uint32_t)x << 1) | 1;   /* bit 0 always set -> bsf(v2) == 0 */
    uint32_t r1, r2;

    __asm__ volatile("bsr %1, %0" : "=r"(r1) : "r"(v1));
    __asm__ volatile("bsf %1, %0" : "=r"(r2) : "r"(v2));

    /* Both results are constant, so these branches must not yield test cases. */
    if (r1 > 100) puts("bsr");
    if (r2 > 100) puts("bsf");
    return 0;
}