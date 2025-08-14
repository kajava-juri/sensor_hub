#include "alarm.h"
#include <stdio.h>

alarm_state_t update_alarm_state(alarm_context_t *ctx, alarm_event_t event) {
    switch (ctx->current_state) {
        case ALARM_STATE_ARMED:
            if (event == EVENT_DOOR_OPEN) {
                ctx->current_state = ALARM_STATE_TRIGGERED;
                alarm_trigger(ctx);
            }
            else if (event == EVENT_DISARM) {
                ctx->current_state = ALARM_STATE_OFF;
            }
            break;
        case ALARM_STATE_TRIGGERED:
            if (event == EVENT_TIMEOUT) {
                ctx->current_state = ALARM_STATE_RESET;
            }
            else if (event == EVENT_RESET) {
                ctx->current_state = ALARM_STATE_RESET;
            }
            break;
        case ALARM_STATE_RESET:
            if (event == EVENT_DISARM) {
                ctx->current_state = ALARM_STATE_OFF;
                alarm_reset(ctx);
            }
            break;
        case ALARM_STATE_OFF:
            if(event == EVENT_ARM) {
                ctx->current_state = ALARM_STATE_ARMED;
            }
        default:
            break;
    }
    return ctx->current_state;
}

void alarm_trigger(alarm_context_t *ctx) {
    printf("Alarm triggered\n");
}

void alarm_reset(alarm_context_t *ctx) {
    printf("Alarm reset\n");
    ctx->door_open_count = 0;
    ctx->armed = false;
}

alarm_context_t alarm_init() {
    alarm_context_t ctx;
    ctx.current_state = ALARM_STATE_ARMED;
    ctx.door_open_count = 0;
    ctx.armed = true;
    return ctx;
}