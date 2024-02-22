#define _GNU_SOURCE
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

    char format_string[100];

    fgets(format_string, 100, file_stream);

    char *formatted_string;
    asprintf(&formatted_string, format_string, 1, 2, 3, 4);
}
