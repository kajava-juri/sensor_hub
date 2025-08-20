#ifndef BUTTONS_H
#define BUTTONS_H

#include <stdint.h>
#include "pico/stdlib.h"
#include <stdbool.h>
#include "common.h"

// GPIO pin assignments for buttons
#define ARM_SWITCH_PIN      3
#define RESET_BUTTON_PIN    4

// Button debounce time in milliseconds
#define BUTTON_DEBOUNCE_MS 50

typedef struct {
    uint32_t last_press_time;
    bool state;
} button_state_t;

typedef struct {
    button_state_t arm_switch;
    button_state_t reset_button;
    alarm_context_t* alarm_ctx;
} button_manager_t;

// Function prototypes
void buttons_init(button_manager_t* manager, alarm_context_t* alarm_ctx);
void button_gpio_callback(uint gpio, uint32_t events);

#endif // BUTTONS_H
