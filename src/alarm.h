#ifndef ALARM_H
#define ALARM_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    ALARM_STATE_OFF,
    ALARM_STATE_ARMED,
    ALARM_STATE_TRIGGERED,
    ALARM_STATE_RESET
} alarm_state_t;

typedef enum {
    EVENT_DOOR_OPEN,
    EVENT_DOOR_CLOSE,
    EVENT_ARM,
    EVENT_DISARM,
    EVENT_TIMEOUT,
    EVENT_RESET
} alarm_event_t;

typedef struct {
    alarm_state_t current_state;
    uint32_t alarm_start_time;
    uint32_t door_open_count;
    bool armed;
} alarm_context_t;

alarm_context_t alarm_init(void);
void alarm_trigger(alarm_context_t *ctx);
void alarm_reset(alarm_context_t *ctx);

alarm_state_t update_alarm_state(alarm_context_t *ctx, alarm_event_t event);

#endif // ALARM_H