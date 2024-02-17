#include <stdio.h>

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

    char input1 = getc(file_stream);
    char input2 = getc(file_stream);
    char input3 = getc(file_stream);

    if (input1 == 'a') {
        puts("foo1");
    }

    if (input2 == 'b') {
        puts("foo2");
    }

    if (input3 == 'c') {
        puts("foo3");
    }

}
