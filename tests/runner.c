#include "greatest.h"
#include <sodium.h>

SUITE_EXTERN(log_suite);
SUITE_EXTERN(role_suite);

GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
    if (sodium_init() < 0) {
        fprintf(stderr, "Failed to initialise libsodium\n");
        return 1;
    }

    GREATEST_MAIN_BEGIN();
    RUN_SUITE(log_suite);
    RUN_SUITE(role_suite);
    GREATEST_MAIN_END();
}
