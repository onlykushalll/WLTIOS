#include <stdio.h>
#include <stdlib.h>
#include <math.h>
int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: wlt-calc <number>\n");
        return 1;
    }
    double n = atof(argv[1]);
    printf("sqrt(%.2f) = %.4f\n", n, sqrt(n));
    return 0;
}
