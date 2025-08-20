#ifndef ALARM_H
#define ALARM_H

#include <stdint.h>
#include <stdbool.h>
#include "common.h"

typedef enum {
    EVENT_ARM,
    EVENT_DISARM,
    EVENT_TIMEOUT,
    EVENT_RESET,
    EVENT_TRIGGER
} alarm_event_t;

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

const char* alarm_state_to_string(alarm_state_t state);

#endif // ALARM_H