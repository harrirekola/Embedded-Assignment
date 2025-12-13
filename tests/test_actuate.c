#include "unity.h"
#include "actuate.h"
#include "config.h"   
#include <pins.h>   

// Ceedling mocks
#include "mock_servo.h"
#include "mock_timers.h"
#include "mock_gpio.h"

void setUp(void) {
    gpio_pin_mode_Ignore();
    gpio_write_Ignore();
    servo_set_pulse_us_Ignore();
    
    actuate_init();
    
    servo_set_pulse_us_StopIgnore();
}

void tearDown(void) {}

// ########## tests for actuate_init ##########

void test_actuate_init_Should_InitializeGPIOs_AndCenterServos(void) {
    gpio_pin_mode_Expect(GPIO_PIN_PRESENCE_LED, GPIO_OUTPUT);
    gpio_write_Expect(GPIO_PIN_PRESENCE_LED, GPIO_LOW);
    gpio_pin_mode_Expect(GPIO_PIN_LED_A, GPIO_OUTPUT);
    gpio_pin_mode_Expect(GPIO_PIN_LED_B, GPIO_OUTPUT);
    gpio_write_Expect(GPIO_PIN_LED_A, GPIO_HIGH);
    gpio_write_Expect(GPIO_PIN_LED_B, GPIO_HIGH);
    
    servo_set_pulse_us_Expect(0, 1500);
    servo_set_pulse_us_Expect(1, 1500);
    servo_set_pulse_us_Expect(2, 1500);
    
    actuate_init();
}

// ########## tests for actuate_fire ##########

void test_actuate_fire_Should_PushCorrectServo_ToActivePosition(void) {
    // Fire POS1
    servo_set_pulse_us_Expect(0, 1700);
    millis_ExpectAndReturn(1000);
    
    actuate_fire(POS1);

    // Fire POS3
    servo_set_pulse_us_Expect(2, 1700);
    millis_ExpectAndReturn(2000);
    
    actuate_fire(POS3);
}

void test_actuate_fire_Should_DriveServo0_ForPos1(void) {
    // POS1 = index 0
    servo_set_pulse_us_Expect(0, 1700);
    millis_ExpectAndReturn(1000);
    
    actuate_fire(POS1);
}

void test_actuate_fire_Should_DriveServo1_ForPos2(void) {
    // POS2 = index 1
    servo_set_pulse_us_Expect(1, 1700);
    millis_ExpectAndReturn(1000);
    
    actuate_fire(POS2);
}

void test_actuate_fire_Should_DriveServo2_ForPos3(void) {
    // POS3 = index 2
    servo_set_pulse_us_Expect(2, 1700);
    millis_ExpectAndReturn(1000);
    
    actuate_fire(POS3);
}

// ########## tests for actuate_tick ##########

void test_actuate_tick_Should_RetrieveServo_AfterDwellTime(void) {
    // Fire the servo at T=1000
    servo_set_pulse_us_Expect(0, 1700);
    millis_ExpectAndReturn(1000);
    actuate_fire(POS1);

    // Tick before expiration T=1249
    actuate_tick(1249);

    // Tick at expiration T=1250
    servo_set_pulse_us_Expect(0, 1500);
    actuate_tick(1250);

    // Tick after expiration T=1251
    actuate_tick(1251);
}

void test_actuate_tick_Should_HandleMultipleServosIndependently(void) {
    // Fire POS1 at T=1000
    servo_set_pulse_us_Expect(0, 1700);
    millis_ExpectAndReturn(1000);
    actuate_fire(POS1);

    // Fire POS2 at T=1100
    servo_set_pulse_us_Expect(1, 1700);
    millis_ExpectAndReturn(1100);
    actuate_fire(POS2);

    // T=1250: POS1 centers (1000+250), POS2 stays active
    servo_set_pulse_us_Expect(0, 1500); 
    actuate_tick(1250);

    // T=1300: Nothing happens
    actuate_tick(1300);

    // T=1350: POS2 centers (1100+250)
    servo_set_pulse_us_Expect(1, 1500);
    actuate_tick(1350);
}

// ########## tests for counters ##########

void test_counters_Should_IncrementAndRetrieveCorrectly(void) {
    counters_reset();
    
    counters_inc_total();
    counters_inc_total();
    
    counters_inc_diverted();
    
    counters_inc_red();
    counters_inc_green();
    counters_inc_green();
    
    const Counters* c = counters_get();
    
    TEST_ASSERT_EQUAL_UINT32(2, c->total);
    TEST_ASSERT_EQUAL_UINT32(1, c->diverted);
    TEST_ASSERT_EQUAL_UINT32(0, c->passed);
    TEST_ASSERT_EQUAL_UINT32(1, c->red);
    TEST_ASSERT_EQUAL_UINT32(2, c->green);
}