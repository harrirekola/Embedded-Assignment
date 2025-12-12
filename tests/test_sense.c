#define TESTING 1

#include "unity.h"
#include "sense.h"
#include "config.h"
#include "pins.h"

// Ceedling mocks
#include "mock_timers.h"
#include "mock_gpio.h"
#include "mock_interrupts.h"
#include "mock_decide.h"
#include "mock_apds9960.h" 
#include "mock_vl6180.h" 
#include "mock_uart.h"

// Setup/teardown before/after each test
void setUp(void) {}
void tearDown(void) {}

void test_sense_init_Should_InitializeSensorsAndState(void) {
    uart_write_IgnoreAndReturn(1);  // some logging
    // The sensors are expected to be initialized
    vl6180_init_ExpectAndReturn(true);  // ToF
    uart_write_IgnoreAndReturn(1);  // logging
    vl6180_config_threshold_mm_ExpectAndReturn(TOF_THRESHOLD_MM, TOF_HYST_MM, true);
    uart_write_IgnoreAndReturn(1);  // logging
    apds9960_init_ExpectAndReturn(true);  // Color
    uart_write_IgnoreAndReturn(1);  // logging

    // Current time should be called
    millis_ExpectAndReturn(100);

    // Set some values to non-zero to verify they are reset
    set_session_active(1);
    set_current_event((DetectEvent){1, 1, 1});
    set_last_interrupt(1);
    set_last_color_sample(0); // this is an exeption
    set_above_count(1);
    set_col_sums((uint32_t[]){1,1,1,1});
    set_color_sample_count(1);

    sense_init();

    // Verify internal variables after initialization
    TEST_ASSERT_EQUAL_UINT8(0, get_session_active());
    TEST_ASSERT_EQUAL_UINT8(0, get_current_event().present);
    TEST_ASSERT_EQUAL_UINT32(0, get_last_interrupt_ms());
    TEST_ASSERT_NOT_EQUAL_UINT32(0, get_last_color_sample_ms()); // time shouldn't be zero anymore
    TEST_ASSERT_EQUAL_UINT16(0, get_above_count());
    TEST_ASSERT_EQUAL_UINT32(0, get_col_r_sum());
    TEST_ASSERT_EQUAL_UINT32(0, get_col_g_sum());
    TEST_ASSERT_EQUAL_UINT32(0, get_col_b_sum());
    TEST_ASSERT_EQUAL_UINT32(0, get_col_c_sum());
    TEST_ASSERT_EQUAL_UINT16(0, get_color_sample_count());
}

// For testing computing small block's length
// Doesn't test if exit is before enter (categorized as small)
void test_sense_compute_length_Should_CategorizeSmall(void) {
    uint32_t max_time_ms = (uint32_t)((LENGTH_SMALL_MAX_MM * 1000U) / BELT_MM_PER_S);  // ~909ms

    uint32_t enter_times[] = {1, 5000, 30000};
    uint32_t dtimes[] = {0, 1, 500, 908, 909}; // all under max_time_ms
    LengthInfo lenin = {0};

    for (size_t i=0; i<3; i++) {
        for (size_t j=0; j<5; j++) {
            decide_get_belt_mm_per_s_ExpectAndReturn(BELT_MM_PER_S);
            t_compute_length(enter_times[i], enter_times[i] + dtimes[j], &lenin);
            char* category = (lenin.cls == LEN_SMALL) ? "LEN_SMALL" : "LEN_NOT_SMALL";
            TEST_ASSERT_EQUAL_STRING("LEN_SMALL", category);
            TEST_ASSERT_LESS_THAN_UINT16(LENGTH_SMALL_MAX_MM, lenin.length_mm);
            TEST_ASSERT_EQUAL_UINT32(lenin.dwell_ms, dtimes[j]);
        }
    }
}

// For testing computing large block's length
void test_sense_compute_length_Should_CategorizeLarge(void) {
    uint32_t enter_times[] = {1, 2000, 30000};
    uint32_t dtimes[] = {910, 100000}; // all above max_time_ms
    LengthInfo lenin = {0};

    for (size_t i=0; i<3; i++) {
        for (size_t j=0; j<2; j++) {
            decide_get_belt_mm_per_s_ExpectAndReturn(BELT_MM_PER_S);
            t_compute_length(enter_times[i], enter_times[i] + dtimes[j], &lenin);
            TEST_ASSERT_EQUAL_UINT32(lenin.dwell_ms, dtimes[j]);
            TEST_ASSERT_GREATER_THAN_UINT16(LENGTH_SMALL_MAX_MM-1, lenin.length_mm);  // emulating >=
            char* category = (lenin.cls == LEN_SMALL) ? "LEN_SMALL" : "LEN_NOT_SMALL";
            TEST_ASSERT_EQUAL_STRING("LEN_NOT_SMALL", category);
        }
    }
}

// Test resetting color accumulation
void test_sense_reset_color_accum_Should_ResetAllSumsAndCount(void) {
    // Set some values to non-zero to verify they are reset
    set_col_sums((uint32_t[]){1,1,1,1});
    set_color_sample_count(1);

    t_reset_color_accum();

    TEST_ASSERT_EQUAL_UINT32(0, get_col_r_sum());
    TEST_ASSERT_EQUAL_UINT32(0, get_col_g_sum());
    TEST_ASSERT_EQUAL_UINT32(0, get_col_b_sum());
    TEST_ASSERT_EQUAL_UINT32(0, get_col_c_sum());
    TEST_ASSERT_EQUAL_UINT16(0, get_color_sample_count());
}

// Test the accumulation of a valid color sample
void test_sense_accumulate_color_sample_Should_AccumulateValidSample(void) {
    // Set initial sums and count
    set_col_sums((uint32_t[]){10, 20, 30, 40});
    set_color_sample_count(2);

    // Mock for a valid color reading
    uint16_t r=5, g=15, b=25, c=35;
    apds9960_read_rgbc_ExpectAnyArgsAndReturn(true);
    apds9960_read_rgbc_ReturnThruPtr_r(&r);
    apds9960_read_rgbc_ReturnThruPtr_g(&g);
    apds9960_read_rgbc_ReturnThruPtr_b(&b);
    apds9960_read_rgbc_ReturnThruPtr_c(&c);

    t_accumulate_color_sample();

    TEST_ASSERT_EQUAL_UINT32(15, get_col_r_sum()); // 10 + 5
    TEST_ASSERT_EQUAL_UINT32(35, get_col_g_sum()); // 20 + 15
    TEST_ASSERT_EQUAL_UINT32(55, get_col_b_sum()); // 30 + 25
    TEST_ASSERT_EQUAL_UINT32(75, get_col_c_sum()); // 40 + 35
    TEST_ASSERT_EQUAL_UINT16(3, get_color_sample_count()); // 2 + 1
}

// Test the accumulation of an invalid color sample
void test_sense_accumulate_color_sample_Should_AccumulateInValidSample(void) {
    // Set initial sums and count
    set_col_sums((uint32_t[]){10, 20, 30, 40});
    set_color_sample_count(2);

    // Mock an invalid color reading
    apds9960_read_rgbc_IgnoreAndReturn(false);

    t_accumulate_color_sample();

    TEST_ASSERT_EQUAL_UINT32(10, get_col_r_sum()); // 10 + 0
    TEST_ASSERT_EQUAL_UINT32(20, get_col_g_sum()); // 20 + 0
    TEST_ASSERT_EQUAL_UINT32(30, get_col_b_sum()); // 30 + 0
    TEST_ASSERT_EQUAL_UINT32(40, get_col_c_sum()); // 40 + 0
    TEST_ASSERT_EQUAL_UINT16(2, get_color_sample_count()); // 2 + 0
}

// Test ending a session
void test_sense_end_session_Should_SetInactiveTurnOffLed(void) {
    uint32_t end_ms = 10500;

    // Set initial values to validate they are changed
    set_session_active(1);
    set_current_event((DetectEvent){1, 10000, 0});
    set_above_count(6);
    set_col_sums((uint32_t[]){50, 60, 70, 80});
    set_color_sample_count(10);

    // Expect to turn off presence LED
    gpio_write_Expect(GPIO_PIN_PRESENCE_LED, GPIO_LOW);

    t_end_session(end_ms);

    TEST_ASSERT_EQUAL_UINT8(0, get_session_active());
    TEST_ASSERT_EQUAL_UINT8(0, get_current_event().present);
    TEST_ASSERT_EQUAL_UINT32(end_ms, get_current_event().t_exit_ms);
    TEST_ASSERT_EQUAL_UINT16(0, get_above_count());

    // Color accumulation should not be affected
    TEST_ASSERT_EQUAL_UINT32(50, get_col_r_sum());
    TEST_ASSERT_EQUAL_UINT32(60, get_col_g_sum());
    TEST_ASSERT_EQUAL_UINT32(70, get_col_b_sum());
    TEST_ASSERT_EQUAL_UINT32(80, get_col_c_sum());
    TEST_ASSERT_EQUAL_UINT16(10, get_color_sample_count());
}

// Test session ending logic
void test_sense_session_should_end_Should_EndSession(void) {
    uint32_t now_ms = 20000;

    set_session_active(1);
    
    uint32_t dtimes[] = {1, 100, 1000};  // how much beyond timeout limit
    for (size_t i=0; i<3; i++) {
        set_last_interrupt(now_ms - (VL6180_QUIET_TIMEOUT_MS + dtimes[i]));
        TEST_ASSERT_TRUE(t_session_should_end(now_ms));
    }
}

void test_sense_session_should_end_ShouldNot_EndSession(void) {
    uint32_t now_ms = 20000;

    // If session is not active, shouldn't end even with timeout
    set_session_active(0);
    set_last_interrupt(now_ms - (VL6180_QUIET_TIMEOUT_MS + 1000));
    TEST_ASSERT_FALSE(t_session_should_end(now_ms));

    set_session_active(1);
    uint32_t dtimes[] = {0, 1, 100};  // how much "within" timeout limit
    for (size_t i=0; i<3; i++) {
        set_last_interrupt(now_ms - (VL6180_QUIET_TIMEOUT_MS - dtimes[i]));
        TEST_ASSERT_FALSE(t_session_should_end(now_ms));
    }
}



// Integration (like) tests: functions call internal methods //

// Test starting a session
void test_sense_start_session_Should_SetActiveAndInitializeEvent(void) {
    uint32_t now_ms = 10000;

    // Set initial values to validate they are changed
    set_session_active(0);
    set_current_event((DetectEvent){0, 0, 0});
    set_above_count(5);
    set_col_sums((uint32_t[]){50, 60, 70, 80});
    set_color_sample_count(10);

    // Expect to turn on presence LED
    gpio_write_Expect(GPIO_PIN_PRESENCE_LED, GPIO_HIGH);

    t_start_session(now_ms);

    TEST_ASSERT_EQUAL_UINT8(1, get_session_active());
    TEST_ASSERT_EQUAL_UINT8(1, get_current_event().present);
    TEST_ASSERT_EQUAL_UINT32(now_ms, get_current_event().t_enter_ms);
    TEST_ASSERT_EQUAL_UINT16(0, get_above_count());

    // Color accumulation should be reset
    TEST_ASSERT_EQUAL_UINT32(0, get_col_r_sum());
    TEST_ASSERT_EQUAL_UINT32(0, get_col_g_sum());
    TEST_ASSERT_EQUAL_UINT32(0, get_col_b_sum());
    TEST_ASSERT_EQUAL_UINT32(0, get_col_c_sum());
    TEST_ASSERT_EQUAL_UINT16(0, get_color_sample_count());
}

// Testing finalization
// Testing ambituous and large only for red to reduce amount of tests

// Test finalizing red, unambiguous, small
void test_sense_finalize_result_RedUnambiguousSmall(void) {
    set_current_event((DetectEvent){0, 10000, 10500}); // 500ms dwell (small)
    set_col_sums((uint32_t[]){500, 100, 50, 600}); // avg r=50, g=10, b=5, c=60
    set_color_sample_count(10);
    SenseResult out = {0};

    decide_get_belt_mm_per_s_ExpectAndReturn(BELT_MM_PER_S);
    apds9960_classify_ExpectAndReturn(50, 10, 5, 60, COLOR_RED);
    uart_write_ExpectAndReturn("color: n=10 r=50 g=10 b=5 c=60 class=0 amb=0\r\n", 1);

    t_finalize_result(&out);

    TEST_ASSERT_EQUAL_UINT32(500, out.length.dwell_ms);
    TEST_ASSERT_EQUAL_UINT8(LEN_SMALL, out.length.cls);
    TEST_ASSERT_EQUAL_UINT8(COLOR_RED, out.color);
    TEST_ASSERT_FALSE(out.ambiguous);
}

// Test finalizing red, ambiguous, small
void test_sense_finalize_result_RedAmbiguousSmall(void) {
    set_current_event((DetectEvent){0, 10000, 10500}); // 500ms dwell (small)
    set_col_sums((uint32_t[]){500, 100, 50, 499}); // avg r=50, g=10, b=5, c=49
    set_color_sample_count(10);
    SenseResult out = {0};

    decide_get_belt_mm_per_s_ExpectAndReturn(BELT_MM_PER_S);
    apds9960_classify_ExpectAndReturn(50, 10, 5, 49, COLOR_RED);
    uart_write_ExpectAndReturn("color: n=10 r=50 g=10 b=5 c=49 class=0 amb=1\r\n", 1);

    t_finalize_result(&out);

    TEST_ASSERT_EQUAL_UINT32(500, out.length.dwell_ms);
    TEST_ASSERT_EQUAL_UINT8(LEN_SMALL, out.length.cls);
    TEST_ASSERT_EQUAL_UINT8(COLOR_RED, out.color);
    TEST_ASSERT_TRUE(out.ambiguous);
}

// Test finalizing red, unambiguous, large
void test_sense_finalize_result_RedUnambiguousLarge(void) {
    set_current_event((DetectEvent){0, 10000, 11000}); // 1000ms dwell (not small)
    set_col_sums((uint32_t[]){500, 100, 50, 600}); // avg r=50, g=10, b=5, c=60
    set_color_sample_count(10);
    SenseResult out = {0};

    decide_get_belt_mm_per_s_ExpectAndReturn(BELT_MM_PER_S);
    apds9960_classify_ExpectAndReturn(50, 10, 5, 60, COLOR_RED);
    uart_write_ExpectAndReturn("color: n=10 r=50 g=10 b=5 c=60 class=0 amb=0\r\n", 1);

    t_finalize_result(&out);

    TEST_ASSERT_EQUAL_UINT32(1000, out.length.dwell_ms);
    TEST_ASSERT_EQUAL_UINT8(LEN_NOT_SMALL, out.length.cls);
    TEST_ASSERT_EQUAL_UINT8(COLOR_RED, out.color);
    TEST_ASSERT_FALSE(out.ambiguous);
}

// Test finalizing green
void test_sense_finalize_result_Green(void) {
    set_current_event((DetectEvent){0, 10000, 10500}); // 500ms dwell (small)
    set_col_sums((uint32_t[]){100, 500, 50, 600}); // avg r=10, g=50, b=5, c=60
    set_color_sample_count(10);
    SenseResult out = {0};

    decide_get_belt_mm_per_s_ExpectAndReturn(BELT_MM_PER_S);
    apds9960_classify_ExpectAndReturn(10, 50, 5, 60, COLOR_GREEN);
    uart_write_ExpectAndReturn("color: n=10 r=10 g=50 b=5 c=60 class=1 amb=0\r\n", 1);

    t_finalize_result(&out);

    TEST_ASSERT_EQUAL_UINT32(500, out.length.dwell_ms);
    TEST_ASSERT_EQUAL_UINT8(LEN_SMALL, out.length.cls);
    TEST_ASSERT_EQUAL_UINT8(COLOR_GREEN, out.color);
    TEST_ASSERT_FALSE(out.ambiguous);
}

// Test finalizing blue
void test_sense_finalize_result_Blue(void) {
    set_current_event((DetectEvent){0, 10000, 10500}); // 500ms dwell (small)
    set_col_sums((uint32_t[]){100, 50, 500, 600}); // avg r=10, g=5, b=50, c=60
    set_color_sample_count(10);
    SenseResult out = {0};

    decide_get_belt_mm_per_s_ExpectAndReturn(BELT_MM_PER_S);
    apds9960_classify_ExpectAndReturn(10, 5, 50, 60, COLOR_BLUE);
    uart_write_ExpectAndReturn("color: n=10 r=10 g=5 b=50 c=60 class=2 amb=0\r\n", 1);

    t_finalize_result(&out);

    TEST_ASSERT_EQUAL_UINT32(500, out.length.dwell_ms);
    TEST_ASSERT_EQUAL_UINT8(LEN_SMALL, out.length.cls);
    TEST_ASSERT_EQUAL_UINT8(COLOR_BLUE, out.color);
    TEST_ASSERT_FALSE(out.ambiguous);
}

// Test finalizing other
void test_sense_finalize_result_Other(void) {
    set_current_event((DetectEvent){0, 10000, 10500}); // 500ms dwell (small)
    set_col_sums((uint32_t[]){100, 100, 100, 600}); // avg r=10, g=10, b=10, c=60
    set_color_sample_count(10);
    SenseResult out = {0};

    decide_get_belt_mm_per_s_ExpectAndReturn(BELT_MM_PER_S);
    apds9960_classify_ExpectAndReturn(10, 10, 10, 60, COLOR_OTHER);
    uart_write_ExpectAndReturn("color: n=10 r=10 g=10 b=10 c=60 class=3 amb=0\r\n", 1);

    t_finalize_result(&out);

    TEST_ASSERT_EQUAL_UINT32(500, out.length.dwell_ms);
    TEST_ASSERT_EQUAL_UINT8(LEN_SMALL, out.length.cls);
    TEST_ASSERT_EQUAL_UINT8(COLOR_OTHER, out.color);
    TEST_ASSERT_FALSE(out.ambiguous);
}

// Test finalizing when no samples
void test_sense_finalize_result_NoSamples(void) {
    set_current_event((DetectEvent){0, 10000, 10500}); // 500ms dwell (small)
    set_col_sums((uint32_t[]){0,0,0,0});
    set_color_sample_count(0);
    SenseResult out = {0};

    decide_get_belt_mm_per_s_ExpectAndReturn(BELT_MM_PER_S);
    // Should call color sensor if no samples
    uint16_t r=50, g=10, b=5, c=60;
    apds9960_read_rgbc_ExpectAnyArgsAndReturn(true);
    apds9960_read_rgbc_ReturnThruPtr_r(&r);
    apds9960_read_rgbc_ReturnThruPtr_g(&g);
    apds9960_read_rgbc_ReturnThruPtr_b(&b);
    apds9960_read_rgbc_ReturnThruPtr_c(&c);
    apds9960_classify_ExpectAndReturn(50, 10, 5, 60, COLOR_RED);
    uart_write_ExpectAndReturn("color: n=1 r=50 g=10 b=5 c=60 class=0 amb=0\r\n", 1);

    t_finalize_result(&out);

    TEST_ASSERT_EQUAL_UINT32(500, out.length.dwell_ms);
    TEST_ASSERT_EQUAL_UINT8(LEN_SMALL, out.length.cls);
    TEST_ASSERT_EQUAL_UINT8(COLOR_RED, out.color);
    TEST_ASSERT_FALSE(out.ambiguous);
}

// Simple integration test finalizing result (small, red, unambiguous)
// Kind of a duplicate, but different structure so kept
void test_sense_finalize_result_Should_PopulateOutput(void) {
    // Set some initial values
    uint16_t length_mm = 40;  // Small block
    uint32_t dwell_ms = length_mm * 1000U / BELT_MM_PER_S + 1; // ~727ms +1 to avoid int truncation on division issues
    DetectEvent de = {1, 15000, 0};
    de.t_exit_ms = de.t_enter_ms + dwell_ms;
    set_current_event(de);

    SenseResult out = {0};

    // On average r=50, g=15, b=5, c=50
    uint32_t col_sums[] = {500, 150, 50, 500};
    uint16_t count = 10;
    uint16_t r = col_sums[0]/count, g = col_sums[1]/count, b = col_sums[2]/count, c = col_sums[3]/count;
    set_col_sums(col_sums);
    set_color_sample_count(count);

    decide_get_belt_mm_per_s_ExpectAndReturn(BELT_MM_PER_S);

    // Mock for color classification
    apds9960_classify_ExpectAndReturn(r, g, b, c, COLOR_RED);

    // Some logging is expected
    uart_write_ExpectAnyArgsAndReturn(1);

    t_finalize_result(&out);

    TEST_ASSERT_EQUAL_UINT8(de.present, out.ev.present);       
    TEST_ASSERT_EQUAL_UINT32(de.t_enter_ms, out.ev.t_enter_ms);
    TEST_ASSERT_EQUAL_UINT32(de.t_exit_ms, out.ev.t_exit_ms);
    TEST_ASSERT_EQUAL_UINT32(dwell_ms, out.length.dwell_ms);
    TEST_ASSERT_EQUAL_UINT16(length_mm, out.length.length_mm);
    TEST_ASSERT_EQUAL_UINT8(LEN_SMALL, out.length.cls);
    TEST_ASSERT_EQUAL_UINT8(COLOR_RED, out.color);
    TEST_ASSERT_FALSE(out.ambiguous);
}


// Test polling.

// Test polling when session not active, no event
void test_sense_poll_NoEventNoSession(void) {
    // Internals
    set_session_active(0);
    
    // Expectations
    millis_ExpectAndReturn(10000);
    vl6180_event_ExpectAndReturn(false);  // no event

    // Execution and verification
    TEST_ASSERT_FALSE(sense_poll(NULL));  // should not finalize
    TEST_ASSERT_FALSE(get_session_active());
}

// Test polling when session not active, evet occurs
void test_sense_poll_EventNoSession(void) {
    // Internals
    uint32_t now = 10000;
    set_session_active(0);
    set_last_color_sample(now);  // to avoid immediate sampling

    // Expectations
    millis_ExpectAndReturn(now);
    
    vl6180_event_ExpectAndReturn(true);  // if event...
        vl6180_read_status_range_ExpectAnyArgsAndReturn(true); // arguments are never used
        vl6180_clear_interrupt_Expect();
        gpio_write_Expect(GPIO_PIN_PRESENCE_LED, GPIO_HIGH); // LED on
        uart_write_ExpectAnyArgsAndReturn(1);

    // Execution and verification
    TEST_ASSERT_FALSE(sense_poll(NULL));  // should not finalize
    TEST_ASSERT_EQUAL_UINT32(now, get_last_interrupt_ms()); // should be updated
    TEST_ASSERT_TRUE(get_session_active());
}

// Test polling when session active and evet occurs
void test_sense_poll_EventSession(void) {
    // Internals
    uint32_t now = 10000;
    set_session_active(1);
    set_last_color_sample(now);  // to avoid immediate sampling
    set_last_interrupt(now - 500);  // to see the change

    // Expectations
    millis_ExpectAndReturn(now);
    vl6180_event_ExpectAndReturn(true);  // if event
        vl6180_read_status_range_ExpectAnyArgsAndReturn(true);  // arguments are never used
        vl6180_clear_interrupt_Expect();

    // Execution and verification
    TEST_ASSERT_FALSE(sense_poll(NULL));  // should not finalize
    TEST_ASSERT_EQUAL_UINT32(now, get_last_interrupt_ms());  // should be updated
    TEST_ASSERT_TRUE(get_session_active());
}

// Test polling when session active and should end
void test_sense_poll_EndSession(void) {
    // Internals
    uint32_t now = 10000;
    SenseResult out = {0};
    uint32_t last_int = now - VL6180_QUIET_TIMEOUT_MS - 1;
    set_session_active(1);
    set_last_color_sample(now);  // to avoid immediate sampling
    set_last_interrupt(last_int);  // to end
    set_col_sums((uint32_t[]){50,0,0,50}); // avg r=50, g=0, b=0, c=50
    set_color_sample_count(1);
    set_current_event((DetectEvent){1, last_int-500, last_int});  // dwell 500ms (small)

    // Expectations
    millis_ExpectAndReturn(now);
    vl6180_event_ExpectAndReturn(false);  // no new event

    gpio_write_Expect(GPIO_PIN_PRESENCE_LED, GPIO_LOW); // end -> LED off
    decide_get_belt_mm_per_s_ExpectAndReturn(BELT_MM_PER_S);
    apds9960_classify_ExpectAndReturn(50, 0, 0, 50, COLOR_RED);
    uart_write_ExpectAnyArgsAndReturn(1);

    // Execution and verification
    TEST_ASSERT_TRUE(sense_poll(&out));  // should end
    TEST_ASSERT_FALSE(get_session_active());
    TEST_ASSERT_EQUAL_UINT32(last_int, out.ev.t_exit_ms);  // test output...
    TEST_ASSERT_EQUAL_UINT8(COLOR_RED, out.color);
    TEST_ASSERT_FALSE(out.ambiguous);
    TEST_ASSERT_EQUAL_UINT8(LEN_SMALL, out.length.cls);
}

// Test polling when session active and accumulate colors
void test_sense_poll_Accumulate(void) {
    // Internals
    uint32_t now = 10000;
    set_session_active(1);
    set_last_color_sample(now - VL6180_MEAS_PERIOD_MS);  // to sample
    set_last_interrupt(now);  // not to end
    set_col_sums((uint32_t[]){0,0,0,0});
    set_color_sample_count(0);

    // Expectations
    millis_ExpectAndReturn(now);
    vl6180_event_ExpectAndReturn(false);  // no new event

    apds9960_read_rgbc_ExpectAnyArgsAndReturn(true);

    // Execution and verification
    TEST_ASSERT_FALSE(sense_poll(NULL));  // should finalize
    TEST_ASSERT_EQUAL_UINT32(now, get_last_color_sample_ms()); // should be updated
    TEST_ASSERT_EQUAL_UINT16(1, get_color_sample_count()); // should be incremented
}

// Longer integration test for polling multiple steps
void test_sense_poll_FullCycle(void) {
    uint32_t time = 100;
    SenseResult sr = {0};

// Initialize the sense
    uart_write_ExpectAnyArgsAndReturn(1);
    vl6180_init_ExpectAndReturn(true);
    uart_write_ExpectAnyArgsAndReturn(1);
    vl6180_config_threshold_mm_ExpectAndReturn(TOF_THRESHOLD_MM, TOF_HYST_MM, true);
    uart_write_ExpectAnyArgsAndReturn(1);
    apds9960_init_ExpectAndReturn(true);
    uart_write_ExpectAnyArgsAndReturn(1);
    millis_ExpectAndReturn(time);
    sense_init();


// 1. poll, no event, no accumulation
    time += 10;
    millis_ExpectAndReturn(time);

    vl6180_event_ExpectAndReturn(false);

    TEST_ASSERT_FALSE(sense_poll(&sr));  // polling
    TEST_ASSERT_FALSE(get_session_active());


// 2. poll, event occurs
    time += 10;
    millis_ExpectAndReturn(time);

    vl6180_event_ExpectAndReturn(true);  // event
        vl6180_read_status_range_ExpectAnyArgsAndReturn(true);
        vl6180_clear_interrupt_Expect();
        uint32_t start = time;
        gpio_write_Expect(GPIO_PIN_PRESENCE_LED, GPIO_HIGH); // LED on
        uart_write_ExpectAnyArgsAndReturn(1);

    TEST_ASSERT_FALSE(sense_poll(&sr));  // polling
    TEST_ASSERT_TRUE(get_session_active());
    TEST_ASSERT_EQUAL_UINT32(time, get_last_interrupt_ms());


// 3. poll, another event occurs
    time += 10;
    millis_ExpectAndReturn(time);

    vl6180_event_ExpectAndReturn(true);  // event
        vl6180_read_status_range_ExpectAnyArgsAndReturn(true);
        vl6180_clear_interrupt_Expect();

    TEST_ASSERT_FALSE(sense_poll(&sr));  // polling
    TEST_ASSERT_TRUE(get_session_active());
    TEST_ASSERT_EQUAL_UINT32(time, get_last_interrupt_ms());


// 4. poll, no event, time to accumulate color
    time += VL6180_MEAS_PERIOD_MS;
    millis_ExpectAndReturn(time);

    vl6180_event_ExpectAndReturn(false);  // no event

    // Accumulation
        uint16_t r=50, g=15, b=5, c=60;
        apds9960_read_rgbc_ExpectAnyArgsAndReturn(true);
         apds9960_read_rgbc_ReturnThruPtr_r(&r);
         apds9960_read_rgbc_ReturnThruPtr_g(&g);
         apds9960_read_rgbc_ReturnThruPtr_b(&b);
         apds9960_read_rgbc_ReturnThruPtr_c(&c);

    TEST_ASSERT_FALSE(sense_poll(&sr));  // polling
    TEST_ASSERT_NOT_EQUAL_UINT32(time, get_last_interrupt_ms());
    TEST_ASSERT_EQUAL_UINT32(time, get_last_color_sample_ms());


// 5. poll, new event, another accumulation
    time += VL6180_MEAS_PERIOD_MS;
    millis_ExpectAndReturn(time);

    vl6180_event_ExpectAndReturn(true);  // event
        vl6180_read_status_range_ExpectAnyArgsAndReturn(true);
        vl6180_clear_interrupt_Expect();    

    // Accumulation
        r=150, g=50, b=15, c=200;
        apds9960_read_rgbc_ExpectAnyArgsAndReturn(true);
         apds9960_read_rgbc_ReturnThruPtr_r(&r);
         apds9960_read_rgbc_ReturnThruPtr_g(&g);
         apds9960_read_rgbc_ReturnThruPtr_b(&b);
         apds9960_read_rgbc_ReturnThruPtr_c(&c);

    TEST_ASSERT_FALSE(sense_poll(&sr));


// Couple artificial accumulations to speed things up
    // Verify accumulation so far
    r=200, g=65, b=20, c=260; // totals
    TEST_ASSERT_EQUAL_UINT16(r, get_col_r_sum());
    TEST_ASSERT_EQUAL_UINT16(g, get_col_g_sum());
    TEST_ASSERT_EQUAL_UINT16(b, get_col_b_sum());
    TEST_ASSERT_EQUAL_UINT16(c, get_col_c_sum());
    TEST_ASSERT_EQUAL_UINT16(2, get_color_sample_count());

    r=2500, g=605, b=270, c=5260; // new totals
    set_col_sums((uint32_t[]){r, g, b, c});
    set_color_sample_count(11);


// 6. poll, last event, no accumulation
    time += 10;
    millis_ExpectAndReturn(time);

    vl6180_event_ExpectAndReturn(true);  // event
        vl6180_read_status_range_ExpectAnyArgsAndReturn(true);
        vl6180_clear_interrupt_Expect();
        uint32_t last_int = time;

    TEST_ASSERT_FALSE(sense_poll(&sr));  // polling

    TEST_ASSERT_EQUAL_UINT16(r, get_col_r_sum());
    TEST_ASSERT_EQUAL_UINT16(g, get_col_g_sum());
    TEST_ASSERT_EQUAL_UINT16(b, get_col_b_sum());
    TEST_ASSERT_EQUAL_UINT16(c, get_col_c_sum());
    TEST_ASSERT_EQUAL_UINT16(11, get_color_sample_count());


// 7. poll, another accumulation
    // No events from this point forward, block exited
    time += VL6180_MEAS_PERIOD_MS;
    millis_ExpectAndReturn(time);

    vl6180_event_ExpectAndReturn(false);  // no event

    // Accumulation
        r=30, g=45, b=75, c=40;
        apds9960_read_rgbc_ExpectAnyArgsAndReturn(true);
         apds9960_read_rgbc_ReturnThruPtr_r(&r);
         apds9960_read_rgbc_ReturnThruPtr_g(&g);
         apds9960_read_rgbc_ReturnThruPtr_b(&b);
         apds9960_read_rgbc_ReturnThruPtr_c(&c);

    TEST_ASSERT_FALSE(sense_poll(&sr));

    r=2500+30, g=605+45, b=270+75, c=5260+40; // new totals
    TEST_ASSERT_EQUAL_UINT16(r, get_col_r_sum());
    TEST_ASSERT_EQUAL_UINT16(g, get_col_g_sum());
    TEST_ASSERT_EQUAL_UINT16(b, get_col_b_sum());
    TEST_ASSERT_EQUAL_UINT16(c, get_col_c_sum());
    TEST_ASSERT_EQUAL_UINT16(12, get_color_sample_count());
    

// 8. poll, no accumulation, don't end yet
    time += VL6180_MEAS_PERIOD_MS-10;
    millis_ExpectAndReturn(time);

    vl6180_event_ExpectAndReturn(false);  // no event

    TEST_ASSERT_FALSE(sense_poll(&sr));

    TEST_ASSERT_EQUAL_UINT16(r, get_col_r_sum());
    TEST_ASSERT_EQUAL_UINT16(g, get_col_g_sum());
    TEST_ASSERT_EQUAL_UINT16(b, get_col_b_sum());
    TEST_ASSERT_EQUAL_UINT16(c, get_col_c_sum());
    TEST_ASSERT_EQUAL_UINT16(12, get_color_sample_count());
    // For some reason these double before next accumulation (bug in test or implementation?)
    

// 9. poll, last accumulation, end session now
    time += VL6180_QUIET_TIMEOUT_MS;
    millis_ExpectAndReturn(time);

    vl6180_event_ExpectAndReturn(false);  // no event

    // Accumulation
        r=10, g=40, b=85, c=20;
        apds9960_read_rgbc_ExpectAnyArgsAndReturn(true);
         apds9960_read_rgbc_ReturnThruPtr_r(&r);
         apds9960_read_rgbc_ReturnThruPtr_g(&g);
         apds9960_read_rgbc_ReturnThruPtr_b(&b);
         apds9960_read_rgbc_ReturnThruPtr_c(&c);

    // Should end now
        // Ending
            gpio_write_Expect(GPIO_PIN_PRESENCE_LED, GPIO_LOW);
        
        // Finalize
            decide_get_belt_mm_per_s_ExpectAndReturn(BELT_MM_PER_S);
            r=(2500+30+10), g=(605+45+40), b=(270+75+85), c=(5260+40+20);
            apds9960_classify_ExpectAndReturn(r, g, b, c, COLOR_RED);
            uart_write_ExpectAnyArgsAndReturn(1);


    TEST_ASSERT_TRUE(sense_poll(&sr));

    TEST_ASSERT_EQUAL_UINT16(r, get_col_r_sum());
    TEST_ASSERT_EQUAL_UINT16(g, get_col_g_sum());
    TEST_ASSERT_EQUAL_UINT16(b, get_col_b_sum());
    TEST_ASSERT_EQUAL_UINT16(c, get_col_c_sum());
    TEST_ASSERT_EQUAL_UINT16(13, get_color_sample_count());


// Validate output
    TEST_ASSERT_FALSE(get_session_active());
    TEST_ASSERT_FALSE(sr.ev.present);
    TEST_ASSERT_EQUAL_UINT32(last_int, sr.ev.t_exit_ms);
    TEST_ASSERT_EQUAL_UINT32(last_int-start, sr.length.dwell_ms);  // 120ms
    TEST_ASSERT_EQUAL_UINT16(66, sr.length.length_mm);  // 120ms * 55 mm/s = 66mm
    TEST_ASSERT_EQUAL_UINT8(LEN_SMALL, sr.length.cls);
    TEST_ASSERT_EQUAL_UINT8(COLOR_RED, sr.color);
    TEST_ASSERT_FALSE(sr.ambiguous);
}
