#include <stdio.h>
int main(int argc, char *argv[]) {
    FILE *f = fopen(argv[1], "r");
    if (!f) return 1;
    int x;
    if (fscanf(f, "%d", &x) != 1) return 1;
    if      (x > 100) printf("big\n");
    else if (x < 100) printf("small\n");
    else              printf("equal\n");
    fclose(f);
    return 0;
}
