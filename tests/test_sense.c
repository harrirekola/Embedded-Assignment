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
// Doesn't test if exit is before enter
void test_sense_compute_length_Should_CategorizeSmall(void) {
    uint32_t max_time_ms = (uint32_t)((LENGTH_SMALL_MAX_MM * 1000U) / BELT_MM_PER_S);  // ~909ms

    uint32_t enter_times[] = {1, 5000, 30000};
    uint32_t dtimes[] = {1, 500, 908}; // all under max_time_ms
    LengthInfo lenin = {0};

    for (size_t i=0; i<3; i++) {
        decide_get_belt_mm_per_s_ExpectAndReturn(BELT_MM_PER_S);
        t_compute_length(enter_times[i], enter_times[i] + dtimes[i], &lenin);
        char* category = (lenin.cls == LEN_SMALL) ? "LEN_SMALL" : "LEN_NOT_SMALL";
        TEST_ASSERT_EQUAL_STRING("LEN_SMALL", category);
        TEST_ASSERT_LESS_THAN_UINT16(LENGTH_SMALL_MAX_MM, lenin.length_mm);
        TEST_ASSERT_EQUAL_UINT32(lenin.dwell_ms, dtimes[i]);
    }
}

// For testing computing large block's length
// Doesn't test if exit is before enter
void test_sense_compute_length_Should_CategorizeLarge(void) {
    uint32_t enter_times[] = {1, 2000, 30000};
    uint32_t dtimes[] = {909, 910, 100000}; // all above max_time_ms
    LengthInfo lenin = {0};

    for (size_t i=0; i<3; i++) {
        decide_get_belt_mm_per_s_ExpectAndReturn(BELT_MM_PER_S);
        t_compute_length(enter_times[i], enter_times[i] + dtimes[i], &lenin);
        TEST_ASSERT_EQUAL_UINT32(lenin.dwell_ms, dtimes[i]);
        TEST_ASSERT_GREATER_THAN_UINT16(LENGTH_SMALL_MAX_MM-1, lenin.length_mm);  // emulating >=
        char* category = (lenin.cls == LEN_SMALL) ? "LEN_SMALL" : "LEN_NOT_SMALL";
        TEST_ASSERT_EQUAL_STRING("LEN_NOT_SMALL", category);
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

// Integration
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

// Test ending a session
void test_sense_end_session_Should_SetInactiveAndFinalizeEvent(void) {
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

// Integration
// Test finalizing result (small, red, unambiguous)
// Maybe could use AllPairs? etc. for more combinations
void test_sense_finalize_result_Should_PopulateOutput(void) {
    // Mock for color reading not used here as sample count > 0

    // Set some initial values
    uint16_t length_mm = 40;  // Small block
    uint32_t dwell_ms = length_mm * 1000U / BELT_MM_PER_S; // ~727ms
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
    TEST_ASSERT_EQUAL_UINT8(0, out.ambiguous);
}

// Integration
// Test sense polling
// First start session -> poll ends -> new poll ends session, finalizes
void test_sense_poll_Should_FinalizeSmallRed(void) {
    // Initial internal values
    set_session_active(0);
    set_last_color_sample(19500);
    set_col_sums((uint32_t[]){0,0,0,0});
    set_color_sample_count(0);

    // Mock listing
    uint32_t past = 20000;
    millis_ExpectAndReturn(past);
    vl6180_event_ExpectAndReturn(true);  // event occurred
    vl6180_read_status_range_ExpectAnyArgsAndReturn(true); // arguments are not used
    vl6180_clear_interrupt_Expect();
    gpio_write_Expect(GPIO_PIN_PRESENCE_LED, GPIO_HIGH);  // presence LED on
    uart_write_ExpectAnyArgsAndReturn(1); // logging for block detected

    // Needed inside accumulate_color_sample
    uint16_t r=50, g=15, b=5, c=50;
    apds9960_read_rgbc_ExpectAnyArgsAndReturn(true);
    apds9960_read_rgbc_ReturnThruPtr_r(&r);
    apds9960_read_rgbc_ReturnThruPtr_g(&g);
    apds9960_read_rgbc_ReturnThruPtr_b(&b);
    apds9960_read_rgbc_ReturnThruPtr_c(&c);

    SenseResult out = {0};
    TEST_ASSERT_FALSE(sense_poll(&out));

    uint32_t now = past + VL6180_QUIET_TIMEOUT_MS + 1; // should end
    millis_ExpectAndReturn(now);
    vl6180_event_ExpectAndReturn(false);  // no new events

    // Accumulate color
    apds9960_read_rgbc_ExpectAnyArgsAndReturn(true);
    apds9960_read_rgbc_ReturnThruPtr_r(&r);
    apds9960_read_rgbc_ReturnThruPtr_g(&g);
    apds9960_read_rgbc_ReturnThruPtr_b(&b);
    apds9960_read_rgbc_ReturnThruPtr_c(&c);
    
    // Presence LED off on ending session
    gpio_write_Expect(GPIO_PIN_PRESENCE_LED, GPIO_LOW);
    
    // Finalizing
    decide_get_belt_mm_per_s_ExpectAndReturn(BELT_MM_PER_S);
    apds9960_classify_ExpectAndReturn(r, g, b, c, COLOR_RED);
    uart_write_ExpectAnyArgsAndReturn(1);

    TEST_ASSERT_TRUE(sense_poll(&out));

    // Verify output 
    TEST_ASSERT_FALSE(out.ev.present);  // should've ended 
    TEST_ASSERT_EQUAL_UINT32(past, out.ev.t_enter_ms);
    TEST_ASSERT_EQUAL_UINT32(now, out.ev.t_exit_ms);
    TEST_ASSERT_LESS_THAN_UINT16(LENGTH_SMALL_MAX_MM, out.length.length_mm);
    TEST_ASSERT_GREATER_THAN_UINT16(0, out.length.length_mm);
    char* category = (out.length.cls == LEN_SMALL) ? "LEN_SMALL" : "LEN_NOT_SMALL";
    TEST_ASSERT_EQUAL_STRING("LEN_SMALL", category);
    TEST_ASSERT_EQUAL_UINT8(COLOR_RED, out.color);
    TEST_ASSERT_EQUAL_UINT8(0, out.ambiguous);
    
    // Verify internal state
    TEST_ASSERT_FALSE(get_session_active());
    TEST_ASSERT_EQUAL_UINT32(past, get_current_event().t_enter_ms);
    TEST_ASSERT_EQUAL_UINT32(now, get_current_event().t_exit_ms);
    TEST_ASSERT_EQUAL_UINT32(past, get_last_interrupt_ms());
}
