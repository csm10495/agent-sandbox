/* tests/unit/test_runner.c - Test runner entry point */
#include "framework.h"
#include <stdio.h>

int test_pass = 0, test_fail = 0;

int main(void) {
    printf("MicrOS unit tests\n");
    printf("=================\n");
    RUN_SUITE(string);
    RUN_SUITE(fs);
    RUN_SUITE(vga);
    printf("-----------------\n");
    printf("Passed: %d  Failed: %d\n", test_pass, test_fail);
    return test_fail ? 1 : 0;
}
