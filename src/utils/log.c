/*
 * UART logging utilities
 * ----------------------
 * Provides compact, parseable UART logs for key events. Formats align with
 * docs/specs/.../contracts/serial.md. Examples:
 * - DETECT/CLEAR: edges from ToF sessions with ids.
 * - CLASSIFY: color (R/G/B/Other), length class and mm.
 * - SCHEDULE/ACTUATE/PASS/SCHEDULE_REJECT: routing and actuation lifecycle.
 * - COUNT: periodic counters snapshot.
 */
#include <stdio.h>
#include "platform/config.h"
#include "hal/uart.h"
#include "drivers/tb6600.h"
#include "utils/log.h"

static void u32_to_str(uint32_t v, char* buf){
    // minimal itoa for non-negative integers
    char tmp[10];
    int i = 0;
    if (v == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    while (v > 0 && i < 10) {
        tmp[i++] = '0' + (v % 10);
        v /= 10;
    }
    int j = 0;
    while (i > 0) {
        buf[j++] = tmp[--i];
    }
    buf[j] = '\0';
}

static void write_kv(const char* k, uint32_t v){
    char b[12];
    u32_to_str(v, b);
    uart_write(k);
    uart_write(b);
}

static const char* color_str(Color c){
    switch (c) {
        case COLOR_RED: return "R";
        case COLOR_GREEN: return "G";
        case COLOR_BLUE: return "B";
        default: return "Other";
    }
}

static const char* pos_str(TargetPosition p){
    switch (p) {
        case POS1: return "Pos1";
        case POS2: return "Pos2";
        case POS3: return "Pos3";
        default: return "PassThrough";
    }
}

void log_detect(uint32_t t_ms, uint16_t evt_id){
    uart_write("DETECT t=");
    { char b[12]; u32_to_str(t_ms, b); uart_write(b);} 
    uart_write(" id=");
    { char b0[12]; u32_to_str(evt_id, b0); uart_write(b0);} 
    uart_write("\r\n");
}

void log_clear(uint32_t t_ms, uint16_t evt_id){
    uart_write("CLEAR t=");
    { char b[12]; u32_to_str(t_ms, b); uart_write(b);} 
    uart_write(" id=");
    { char b0[12]; u32_to_str(evt_id, b0); uart_write(b0);} 
    uart_write("\r\n");
}

void log_classify(uint32_t t_ms, Color color, LengthInfo info, uint16_t evt_id){
    uart_write("CLASSIFY t=");
    { char b[12]; u32_to_str(t_ms, b); uart_write(b);} 
    uart_write(" id=");
    { char b0[12]; u32_to_str(evt_id, b0); uart_write(b0);} 
    uart_write(" color=");
    uart_write(color_str(color));
    uart_write(" len_mm=");
    { char b2[12]; u32_to_str(info.length_mm, b2); uart_write(b2);} 
    uart_write(" class=");
    uart_write(info.cls == LEN_SMALL ? "Small" : "NotSmall");
    uart_write(" thr=");
    { char b3[12]; u32_to_str(LENGTH_SMALL_MAX_MM, b3); uart_write(b3);} 
    uart_write("\r\n");
}
void log_schedule(uint32_t t_ms, TargetPosition pos, uint32_t at_ms, uint16_t evt_id){
    uart_write("SCHEDULE t="); { char b[12]; u32_to_str(t_ms,b); uart_write(b);} 
    uart_write(" id="); { char b0[12]; u32_to_str(evt_id,b0); uart_write(b0);} 
    uart_write(" pos="); uart_write(pos_str(pos)); 
    uart_write(" at="); { char b2[12]; u32_to_str(at_ms,b2); uart_write(b2);} uart_write("\r\n");
}
void log_actuate(uint32_t t_ms, TargetPosition pos, uint16_t evt_id){
    uart_write("ACTUATE t=");
    { char b[12]; u32_to_str(t_ms, b); uart_write(b);} 
    uart_write(" id=");
    { char b0[12]; u32_to_str(evt_id, b0); uart_write(b0);} 
    uart_write(" pos=");
    uart_write(pos_str(pos));
    uart_write("\r\n");
}

void log_schedule_reject(uint32_t t_ms, uint16_t evt_id, const char* reason){
    uart_write("SCHEDULE_REJECT t=");
    { char b[12]; u32_to_str(t_ms, b); uart_write(b);} 
    uart_write(" id=");
    { char b0[12]; u32_to_str(evt_id, b0); uart_write(b0);} 
    uart_write(" reason=");
    uart_write(reason ? reason : "unknown");
    uart_write("\r\n");
}

void log_pass(uint32_t t_ms){
    uart_write("PASS t=");
    { char b[12]; u32_to_str(t_ms, b); uart_write(b);} 
    uart_write("\r\n");
}

void log_fault(uint32_t t_ms, const char* code){
    uart_write("FAULT t=");
    { char b[12]; u32_to_str(t_ms, b); uart_write(b);} 
    uart_write(" code=");
    uart_write(code ? code : "Unknown");
    uart_write("\r\n");
}

void log_count(uint32_t t_ms, uint32_t total, uint32_t diverted, uint32_t passed, uint32_t fault,
               uint32_t red, uint32_t green, uint32_t blue, uint32_t other){
    uart_write("COUNT t=");
    { char b[12]; u32_to_str(t_ms, b); uart_write(b);} 
    uart_write(" total=");
    write_kv("", total);
    uart_write(" diverted=");
    write_kv("", diverted);
    uart_write(" passed=");
    write_kv("", passed);
    uart_write(" fault=");
    write_kv("", fault);
    uart_write(" red=");
    write_kv("", red);
    uart_write(" green=");
    write_kv("", green);
    uart_write(" blue=");
    write_kv("", blue);
    uart_write(" other=");
    write_kv("", other);
}

void log_length(uint32_t t_ms, uint16_t length_mm, uint32_t dwell_ms, uint16_t evt_id){
    uart_write("LENGTH t="); { char b[12]; u32_to_str(t_ms,b); uart_write(b);} 
    uart_write(" id="); { char b0[12]; u32_to_str(evt_id,b0); uart_write(b0);} 
    uart_write(" len_mm="); { char b2[12]; u32_to_str(length_mm,b2); uart_write(b2);} 
    uart_write(" dwell_ms="); { char b3[12]; u32_to_str(dwell_ms,b3); uart_write(b3);} 
    uart_write("\r\n");
}

void log_sep(void){
    uart_write("*******\r\n");
}

void log_belt_configuration(void){
    uint16_t step_rate = tb6600_get_step_rate_hz();
    uint32_t mmpp = (uint32_t)MM_PER_PULSE_X1000;
    uint16_t belt_mm_per_s = tb6600_get_speed_mm_per_s();
    // BELT: step_rate=123 Hz, mm_per_pulse=0.031 mm, belt=50 mm/s
    uart_write("BELT: step_rate=");
    char b[12]; u32_to_str(step_rate, b); uart_write(b);
    uart_write(" Hz, mm_per_pulse=");
    char b_int[12]; u32_to_str(mmpp/1000UL, b_int); uart_write(b_int);
    uart_write("."); char b_frac[12]; u32_to_str(mmpp%1000UL, b_frac); uart_write(b_frac);
    uart_write(" mm, belt="); char b_belt[12]; u32_to_str(belt_mm_per_s, b_belt); uart_write(b_belt);
    uart_write(" mm/s\r\n");
}

void log_servo_distances(void){
    // DIST: D1=120mm D2=240mm D3=360mm
    uart_write("DIST: D1="); char b1[12]; u32_to_str(SERVO_D1_MM, b1); uart_write(b1);
    uart_write("mm D2="); char b2[12]; u32_to_str(SERVO_D2_MM, b2); uart_write(b2);
    uart_write("mm D3="); char b3[12]; u32_to_str(SERVO_D3_MM, b3); uart_write(b3);
    uart_write("mm\r\n");
}
