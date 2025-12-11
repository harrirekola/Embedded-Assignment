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

// ########## tests for throughput guardrail ##########

void test_guardrail_Should_Reject_IfTooManyBlocks(void) {
    decide_set_max_blocks_per_min(2);

    // Ignore logging
    log_schedule_Ignore();
    log_schedule_reject_Ignore();

    // 1st block: accepted
    TEST_ASSERT_TRUE(decide_schedule(POS1, 1000, 1));
    
    // 2nd block: accepted
    TEST_ASSERT_TRUE(decide_schedule(POS1, 2000, 2));
    
    // 3rd block: should be REJECTED due to throughput limit
    log_schedule_reject_Expect(3000, 3, "throughput");
    TEST_ASSERT_FALSE(decide_schedule(POS1, 3000, 3));
}

// ########## tests min spacing guardrail ##########

void test_Requirements_Guardrail_MinSpacing(void) {
    decide_set_min_spacing_ms(500); 

    // Simulate that we just fired at T=1000
    // Run a valid cycle first to set internal state
    actuate_fire_Expect(POS1); 
    log_schedule_Ignore(); 
    log_actuate_Ignore();
    decide_schedule(POS1, 0, 1);
    decide_tick(1000);

    // Next item is due at 1200
    // 1200 < (1000 + 500) -> violates spacing
    decide_schedule(POS1, 500, 2);
    // Should NOT fire
    decide_tick(1200);
    
    // Tick at 1600
    // 1600 > (1000 + 500) -> now valid and should fire
    actuate_fire_Expect(POS1);
    log_actuate_Ignore();
    decide_tick(1600);
}

// ########## tests for logging ##########

void test_Logging_SCHEDULE(void) {
    decide_set_belt_mm_per_s(100);

    // Expect SCHEDULE log when a valid item is accepted
    log_schedule_Expect(1000, POS2, 2900, 55); 
    // 240mm dist / 100 speed = 2400ms
    // 2400 - 500 advance = 1900 delay
    // 1000 + 1900 = 2900 due

    decide_schedule(POS2, 1000, 55);
}

void test_Logging_PASS_Rejection(void) {
    // When routing returns PASS_THROUGH, decide_schedule rejects it

    // Expect SCHEDULE_REJECT with reason pass-through
    log_schedule_reject_Expect(1000, 99, "pass-through");

    bool result = decide_schedule(PASS_THROUGH, 1000, 99);
    TEST_ASSERT_FALSE(result);
}