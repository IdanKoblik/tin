#include "greatest.h"

SUITE_EXTERN(log_suite);
SUITE_EXTERN(role_suite);

GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
    GREATEST_MAIN_BEGIN();
    RUN_SUITE(log_suite);
    RUN_SUITE(role_suite);
    GREATEST_MAIN_END();
}
