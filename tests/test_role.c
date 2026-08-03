#include "greatest.h"
#include "net/role.h"

TEST role_host_is_server(void) {
    ASSERT_EQ(SERVER, string_to_role("host"));
    PASS();
}

TEST role_connect_is_client(void) {
    ASSERT_EQ(CLIENT, string_to_role("connect"));
    PASS();
}

TEST role_unrecognized_is_unknown(void) {
    ASSERT_EQ(UNKNOWN, string_to_role("bluh bluh bluh"));
    ASSERT_EQ(UNKNOWN, string_to_role(""));
    PASS();
}

TEST role_matching_is_case_sensitive(void) {
    ASSERT_EQ(UNKNOWN, string_to_role("Host"));
    ASSERT_EQ(UNKNOWN, string_to_role("HOST"));
    ASSERT_EQ(UNKNOWN, string_to_role("Connect"));
    PASS();
}

TEST role_matching_is_exact(void) {
    ASSERT_EQ(UNKNOWN, string_to_role("hos"));
    ASSERT_EQ(UNKNOWN, string_to_role("hosts"));
    ASSERT_EQ(UNKNOWN, string_to_role(" host"));
    ASSERT_EQ(UNKNOWN, string_to_role("connected"));
    PASS();
}

SUITE(role_suite) {
    RUN_TEST(role_host_is_server);
    RUN_TEST(role_connect_is_client);
    RUN_TEST(role_unrecognized_is_unknown);
    RUN_TEST(role_matching_is_case_sensitive);
    RUN_TEST(role_matching_is_exact);
}
