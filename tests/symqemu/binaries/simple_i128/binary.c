#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>

struct uint128_t {
    uint64_t lo;
    uint64_t hi;
} __attribute__ (( __aligned__( 16 ) ));

int main(int argc, char *argv[]) {
    struct uint128_t input = {0};
    size_t nb_read;

    if (argc != 2) {
        puts("ERROR: You need one argument.");
        return 1;
    }

    int fd = open(argv[1], O_RDONLY);

    if (fd == -1) {
        puts("ERROR: Could not open file.");
        return 1;
    }

    nb_read = read(fd, &input, 16);

    if (nb_read != 16) {
        return 1;
    }

    asm goto (
        "movq $0xcafebabecafebabe, %%rdx\n\t"
        "movq $0xdeadbeefdeadbeef, %%rax\n\t"
        "movq $0, %%rcx\n\t"
        "movq $1, %%rbx\n\t"
        "cmpxchg16b %0\n\t"
        "jz %l[equal]"
        : "+m"(input)
        :
        : "cc","rax","rbx","rcx","rdx"
        : equal);

    puts("foo");
    return 0;

equal: 
    puts("bar");
    
    return 0;
}
