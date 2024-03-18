#include <stdio.h>
#include <stdint.h>

struct uint128_t
{
    uint64_t lo;
    uint64_t hi;
} __attribute__ (( __aligned__( 16 ) ));

int main(int argc, char *argv[]) {

    if (argc != 2) {
        puts("ERROR: You need one argument.");
        return 1;
    }

    FILE* file_stream = fopen(argv[1], "r");

    if (!file_stream) {
        puts("ERROR: Could not open file.");
        return 1;
    }

    size_t nb_read;

    struct uint128_t input = {0};

    nb_read = fread(&input.lo, 1, 8, file_stream);

    if (nb_read != 8) {
        return 1;
    }

    nb_read = fread(&input.hi, 1, 8, file_stream);

    if (nb_read != 8) {
        return 1;
    }

    __asm__ ("cmpxchg16b %0\n\t":"+m"(input)::);

    if (input.lo == 0xdeadbeefdeadbeef && input.hi == 0xcafebabecafebabe) {
        puts("foo");
    } else {
        puts("bar");
    }
}
