#include "alarm.h"
#include "pico/time.h"
#include <stdio.h>
#include <stdlib.h>
#include "mqtt.h"

void update_alarm_state(alarm_context_t *ctx, alarm_event_t event) {
    alarm_context_t previous_state = *ctx;
    switch (ctx->current_state) {
        case ALARM_STATE_ARMED:
            if (event == EVENT_TRIGGER) {
                alarm_trigger(ctx);
                ctx->current_state = ALARM_STATE_TRIGGERED;
            }
            else if (event == EVENT_DISARM) {
                ctx->current_state = ALARM_STATE_DISARMED;
            }
            break;
        case ALARM_STATE_TRIGGERED:
            if (event == EVENT_TIMEOUT) {
                ctx->current_state = ALARM_STATE_ARMED;
            }
            else if (event == EVENT_DISARM) {
                ctx->current_state = ALARM_STATE_DISARMED;
            }
            else if (event == EVENT_RESET) {
                // maybe save last state before it got triggered?
                // currently assume that it can only be triggered when armed
                ctx->current_state = ALARM_STATE_ARMED;
            }
            break;
        case ALARM_STATE_DISARMED:
            if (event == EVENT_ARM) {
                ctx->current_state = ALARM_STATE_ARMED;
            }
            break;
        default:
            break;
    }

    if(previous_state.current_state != ctx->current_state) {
        printf("Alarm state changed from %s to %s\n",
               alarm_state_to_string(previous_state.current_state),
               alarm_state_to_string(ctx->current_state));
        mqtt_flags.alarm_state_changed = true;
    }
}

void alarm_trigger(alarm_context_t *ctx) {
    printf("ALARM TRIGGERED!\n");
    ctx->alarm_start_time = to_ms_since_boot(get_absolute_time());
    // TODO: add buzzer, notification, etc.
}

void alarm_reset(alarm_context_t *ctx) {
    printf("Alarm reset\n");
    ctx->current_state = ALARM_STATE_DISARMED;
}

alarm_context_t *alarm_init() {
    alarm_context_t *ctx = calloc(1, sizeof(alarm_context_t));
    ctx->current_state = ALARM_STATE_ARMED;
    ctx->alarm_start_time = 0;
    return ctx;
}

const char* alarm_state_to_string(alarm_state_t state) {
    switch (state) {
        case ALARM_STATE_ARMED: return "ARMED";
        case ALARM_STATE_DISARMED: return "DISARMED";
        case ALARM_STATE_TRIGGERED: return "TRIGGERED";
        default: return "UNKNOWN";
    }
}