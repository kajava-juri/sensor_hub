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
            else if (event == EVENT_ENTRY_DELAY) {
                ctx->current_state = ALARM_STATE_TRIGGERING;
                ctx->enter_delay_active = true;
                add_repeating_timer_ms(ENTRY_DELAY_MS, entry_delay_callback, ctx, &ctx->entry_timer);
            }
            break;
        case ALARM_STATE_ARMING:
            if(event == EVENT_ARM) {
                ctx->current_state = ALARM_STATE_ARMED;
            }
            else if(event == EVENT_DISARM) {
                bool cancelled = cancel_repeating_timer(&ctx->exit_timer);
                if (cancelled) {
                    ctx->exit_delay_active = false;
                    printf("Exit delay cancelled\n");
                }
            }
            else if(event == EVENT_EXIT_DELAY) {
                printf("UHMMM... got exit delay (EVENT_EXIT_DELAY) while arming (EVENT_DISARM), this should not happen\n");
            }
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
            else if (event == EVENT_EXIT_DELAY) {
                // it does not make sense why you would want to delay the disarming from triggered state
                // this is not intended, just in case somewhere I accidentally do, change state immediately
                ctx->current_state = ALARM_STATE_DISARMED;
            }
            break;
        case ALARM_STATE_TRIGGERING:
            if (event == EVENT_TRIGGER) {
                if (ctx->enter_delay_active) {
                    // If entry delay is active, we need to cancel it
                    cancel_repeating_timer(&ctx->entry_timer);
                    ctx->enter_delay_active = false;
                }
                alarm_trigger(ctx);
                ctx->current_state = ALARM_STATE_TRIGGERED;
            }
            else if (event == EVENT_DISARM) {
                if (ctx->enter_delay_active) {
                    // If entry delay is active, we need to cancel it
                    cancel_repeating_timer(&ctx->entry_timer);
                    ctx->enter_delay_active = false;
                }
                ctx->current_state = ALARM_STATE_DISARMED;
            }
            else if (event == EVENT_ENTRY_DELAY) {
                ctx->current_state = ALARM_STATE_TRIGGERING;
                ctx->enter_delay_active = true;
                add_repeating_timer_ms(ENTRY_DELAY_MS, entry_delay_callback, ctx, &ctx->entry_timer);
            }
            else if (event == EVENT_RESET) {
                // Resetting from triggering state goes to armed
                if (ctx->enter_delay_active) {
                    cancel_repeating_timer(&ctx->entry_timer);
                    ctx->enter_delay_active = false;
                }
                ctx->current_state = ALARM_STATE_ARMED;
            }
            break;
        case ALARM_STATE_DISARMED: // cannot go from disarmed -> triggered
            if (event == EVENT_ARM) {
                ctx->current_state = ALARM_STATE_ARMED;
            }
            else if(event == EVENT_EXIT_DELAY) {
                ctx->current_state = ALARM_STATE_ARMING;
                ctx->exit_delay_active = true;
                add_repeating_timer_ms(EXIT_DELAY_MS, exit_delay_callback, ctx, &ctx->exit_timer);
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
        case ALARM_STATE_TRIGGERING: return "TRIGGERING";
        case ALARM_STATE_ARMING: return "ARMING";
        default: return "UNKNOWN";
    }
}

bool exit_delay_callback(struct repeating_timer *rt) {
    alarm_context_t *ctx = (alarm_context_t *) rt->user_data;
    if(ctx->current_state == ALARM_STATE_ARMING) {
        update_alarm_state(ctx, EVENT_ARM);
        printf("Exit delay expired, system about to ARM\n");
    }
    ctx->exit_delay_active = false;
    return false;
}

bool entry_delay_callback(struct repeating_timer *rt) {
    alarm_context_t *ctx = (alarm_context_t *) rt->user_data;
    if(ctx->current_state == ALARM_STATE_TRIGGERING) {
        update_alarm_state(ctx, EVENT_TRIGGER);
        printf("Entry delay expired, system about to TRIGGER\n");
    }
    ctx->enter_delay_active = false;
    return false;
}