#include "unity.h"
#include "decide.h"
#include "config.h"

// Ceedling mocks
#include "mock_actuate.h"
#include "mock_log.h"
#include "mock_tb6600.h" 

void setUp(void) {
    decide_init();
    decide_set_belt_mm_per_s(100);
    // Disable guardrails
    decide_set_min_spacing_ms(0);
    decide_set_max_blocks_per_min(0);
}

void tearDown(void) {}

// ########## tests for decide_route ##########

void test_decide_route_Should_RouteRedSmall_To_Pos1(void) {
    TEST_ASSERT_EQUAL(POS1, decide_route(COLOR_RED, LEN_SMALL));
}

void test_decide_route_Should_RouteGreenSmall_To_Pos2(void) {
    TEST_ASSERT_EQUAL(POS2, decide_route(COLOR_GREEN, LEN_SMALL));
}

void test_decide_route_Should_RouteBlueSmall_To_Pos3(void) {
    TEST_ASSERT_EQUAL(POS3, decide_route(COLOR_BLUE, LEN_SMALL));
}

void test_decide_route_Should_PassThrough_NotSmall(void) {
    TEST_ASSERT_EQUAL(PASS_THROUGH, decide_route(COLOR_RED, LEN_NOT_SMALL));
}

void test_decide_route_Should_PassThrough_OtherColor(void) {
    TEST_ASSERT_EQUAL(PASS_THROUGH, decide_route(COLOR_OTHER, LEN_SMALL));
}