#ifndef ALARM_H
#define ALARM_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    ALARM_STATE_ARMED,
    ALARM_STATE_DISARMED,
    ALARM_STATE_TRIGGERED,
} alarm_state_t;

typedef enum {
    EVENT_ARM,
    EVENT_DISARM,
    EVENT_TIMEOUT,
    EVENT_RESET,
    EVENT_TRIGGER
} alarm_event_t;

typedef struct {
    alarm_state_t current_state;
    uint32_t alarm_start_time;
    uint32_t door_open_count;
    uint32_t trigger_count;  // Track how many times alarm has been triggered
} alarm_context_t;

alarm_context_t* alarm_init(void);
void alarm_trigger(alarm_context_t *ctx);
void alarm_reset(alarm_context_t *ctx);

void update_alarm_state(alarm_context_t *ctx, alarm_event_t event);

// Helper functions
static inline bool alarm_is_armed(const alarm_context_t *ctx) {
    return ctx->current_state == ALARM_STATE_ARMED;
}

static inline bool alarm_is_triggered(const alarm_context_t *ctx) {
    return ctx->current_state == ALARM_STATE_TRIGGERED;
}

static inline const char* alarm_state_to_string(alarm_state_t state) {
    switch (state) {
        case ALARM_STATE_ARMED: return "ARMED";
        case ALARM_STATE_DISARMED: return "DISARMED";
        case ALARM_STATE_TRIGGERED: return "TRIGGERED";
        default: return "UNKNOWN";
    }
}

#endif // ALARM_H