/*
 * Decide module: routing and scheduling
 * -------------------------------------
 * Responsibilities:
 * - Route a classified item (color + length class) to a target diverter position.
 * - Compute when that diverter should fire based on the belt speed and distance
 *   from the sensor to each diverter (SERVO_Dx_MM), with an optional global
 *   ACTUATION_ADVANCE_MS to fire slightly earlier if needed.
 * - Maintain a tiny queue of future actuations. We allow out-of-order scheduling
 *   but enforce minimum spacing at fire time to protect mechanics.
 * - Provide a runtime-adjustable belt speed so length math follows real motion.
 * How it works at a glance:
 * - decide_route: small+color -> POS1/2/3; others pass-through.
 * - decide_schedule: compute due time from detect timestamp and belt speed; enqueue.
 * - decide_tick: at each loop, if any item is due and spacing allows, fire it.
 */
#include "app/decide.h"
#include "platform/config.h"
#include "app/actuate.h"
#include "utils/log.h"

// Inlined scheduler state and config (ring buffer)
typedef struct {
    uint32_t t_due_ms;
    TargetPosition pos;
    uint8_t active;
    uint16_t event_id; // correlates to the originating detection
} ScheduleItem;

static ScheduleItem s_schedule_queue[SCHED_CAPACITY];
static uint32_t s_last_act_ms = 0;
static uint32_t s_last_due_ms = 0;
static uint16_t s_min_spacing_ms = 0;
static uint8_t s_max_blocks_per_min = 0; // 0 = disabled
static uint8_t s_blocks_in_window = 0;
static uint32_t s_window_start_ms = 0;
static uint16_t s_belt_mm_per_s = BELT_MM_PER_S; // runtime adjustable

void decide_init(void) {
    for (uint8_t i = 0; i < SCHED_CAPACITY; i++) {
        s_schedule_queue[i].active = 0;
    }
    s_last_act_ms = 0;
    s_last_due_ms = 0;
}
void decide_set_min_spacing_ms(uint16_t ms) {
    s_min_spacing_ms = ms;
}

void decide_set_max_blocks_per_min(uint8_t bpm) {
    s_max_blocks_per_min = bpm;
}

void decide_set_belt_mm_per_s(uint16_t v) {
    if (v > 0) {
        s_belt_mm_per_s = v;
    }
}
uint16_t decide_get_belt_mm_per_s(void) {
    return s_belt_mm_per_s;
}

// Inlined router
TargetPosition decide_route(Color color, LengthClass cls) {
    if (cls != LEN_SMALL) {
        return PASS_THROUGH;
    }
    switch (color) {
        case COLOR_RED: return POS1;
        case COLOR_GREEN: return POS2;
        case COLOR_BLUE: return POS3;
        default: return PASS_THROUGH;
    }
}

static uint16_t distance_for_position(TargetPosition p) {
    if (p == POS1) { return SERVO_D1_MM; }
    if (p == POS2) { return SERVO_D2_MM; }
    if (p == POS3) { return SERVO_D3_MM; }
    return 0;
}

static int8_t find_free_slot(void) {
    for (uint8_t i = 0; i < SCHED_CAPACITY; i++) {
        if (!s_schedule_queue[i].active) { 
            return (int8_t)i; 
        }
    }
    return -1;
}

bool decide_schedule(TargetPosition pos, uint32_t detect_ms, uint16_t evt_id) {
    if (pos == PASS_THROUGH) { 
        log_schedule_reject(detect_ms, evt_id, "pass-through"); 
        return false; 
    }

    uint16_t d = distance_for_position(pos);
    if (d == 0 || s_belt_mm_per_s == 0) { 
        log_schedule_reject(detect_ms, evt_id, "invalid-config"); 
        return false; 
    }

    uint32_t delay_ms = ((uint32_t)d * 1000U) / s_belt_mm_per_s;
    uint32_t due = detect_ms + delay_ms;
    // Apply global actuation advance (ms), clamped to detection time
    if (ACTUATION_ADVANCE_MS > 0) {
        if (delay_ms > ACTUATION_ADVANCE_MS) {
            due -= ACTUATION_ADVANCE_MS;
        } else {
            due = detect_ms; // cannot schedule before detection
        }
    }
    // Removed schedule-time spacing guard. We allow out-of-order scheduling.
    // Spacing is enforced at fire time in decide_tick() using s_last_act_ms.

    // throughput guardrail within sliding 60s window
    if (s_max_blocks_per_min) {
        if (s_window_start_ms == 0 || (detect_ms - s_window_start_ms) >= 60000U) { s_window_start_ms = detect_ms; s_blocks_in_window = 0; }
        if (s_blocks_in_window >= s_max_blocks_per_min) { log_schedule_reject(detect_ms, evt_id, "throughput"); return false; }
        s_blocks_in_window++;
    }

    int8_t idx = find_free_slot();
    if (idx < 0) { 
        log_schedule_reject(detect_ms, evt_id, "queue-full"); 
        return false; 
    }
    s_schedule_queue[idx].pos = pos;
    s_schedule_queue[idx].t_due_ms = due;
    s_schedule_queue[idx].active = 1;
    s_schedule_queue[idx].event_id = evt_id;
    s_last_due_ms = due;
    log_schedule(detect_ms, pos, due, evt_id);
    return true;
}

void decide_tick(uint32_t now_ms) {
    // Enforce min spacing between actual firings
    if (s_min_spacing_ms && s_last_act_ms && (now_ms < (s_last_act_ms + s_min_spacing_ms))) { 
        return; 
    }
    // Find earliest due item ready to fire
    int8_t best_i = -1;
    uint32_t best_due = 0xFFFFFFFFUL;
    for (uint8_t i = 0; i < SCHED_CAPACITY; i++) {
        if (s_schedule_queue[i].active && s_schedule_queue[i].t_due_ms <= now_ms) {
            if (s_schedule_queue[i].t_due_ms < best_due) { 
                best_due = s_schedule_queue[i].t_due_ms; 
                best_i = i; 
            }
        }
    }

    // No due item found
    if (best_i < 0) { // BUG!
        return; 
    }

    actuate_fire(s_schedule_queue[best_i].pos);
    log_actuate(now_ms, s_schedule_queue[best_i].pos, s_schedule_queue[best_i].event_id);
    s_schedule_queue[best_i].active = 0;
    s_last_act_ms = now_ms;
}

uint32_t decide_last_due_ms(void) {
    return s_last_due_ms;
}

