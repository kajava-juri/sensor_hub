#include "buttons.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "pico/time.h"
#include <stdio.h>

static button_manager_t* g_button_manager = NULL;

void buttons_init(button_manager_t* manager, alarm_context_t* alarm_ctx) {
    manager->alarm_ctx = alarm_ctx;
    manager->arm_switch.state = false;
    manager->reset_button.state = false;

    // Set global reference for callback
    g_button_manager = manager;

    // Initialize ARM switch (with 10K-20K voltage divider: 5V->3.33V, 0V->0V)
    gpio_init(ARM_SWITCH_PIN);
    gpio_set_dir(ARM_SWITCH_PIN, GPIO_IN);
    // No pull resistor needed - voltage divider provides definite voltage levels
    gpio_set_irq_enabled(ARM_SWITCH_PIN, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true);
    
    // Initialize RESET button
    gpio_init(RESET_BUTTON_PIN);
    gpio_set_dir(RESET_BUTTON_PIN, GPIO_IN);
    gpio_pull_up(RESET_BUTTON_PIN);  // Assuming buttons are active low
    gpio_set_irq_enabled(RESET_BUTTON_PIN, GPIO_IRQ_EDGE_FALL, true);

    printf("Buttons initialized: ARM on GPIO %d, RESET on GPIO %d\n", ARM_SWITCH_PIN, RESET_BUTTON_PIN);
    printf("Note: Button interrupts are handled by main GPIO callback\n");
    
    // Read initial switch position to sync with physical state
    sleep_ms(10);  // Allow GPIO to stabilize
    bool initial_switch_state = gpio_get(ARM_SWITCH_PIN);
    printf("Initial ARM switch position: %s\n", initial_switch_state ? "HIGH (ARM position)" : "LOW (DISARM position)");
    update_alarm_state(g_button_manager->alarm_ctx, initial_switch_state ? EVENT_ARM : EVENT_DISARM);
    // Optional: Sync alarm state with physical switch position at startup
    // Uncomment if you want the system to match the physical switch on boot
    /*
    if (initial_switch_state && manager->alarm_ctx->current_state == ALARM_STATE_DISARMED) {
        update_alarm_state(manager->alarm_ctx, EVENT_ARM);
        printf("System synced to ARM position on startup\n");
    }
    */
}

void button_gpio_callback(uint gpio, uint32_t events) {
    if (!g_button_manager) return;
    
    uint32_t current_time = to_ms_since_boot(get_absolute_time());
    
    switch (gpio) {
        case ARM_SWITCH_PIN: {
            // Check debounce
            if (current_time - g_button_manager->arm_switch.last_press_time < BUTTON_DEBOUNCE_MS) {
                return;
            }

            g_button_manager->arm_switch.last_press_time = current_time;
            
            // Read current switch state
            bool switch_high = gpio_get(ARM_SWITCH_PIN);
            printf("ARM switch toggled to: %s\n", switch_high ? "HIGH (3.33V - ARM)" : "LOW (0V - DISARM)");

            if (switch_high) {
                // Switch to HIGH position (3.33V) = ARM
                if (g_button_manager->alarm_ctx->current_state == ALARM_STATE_DISARMED) {
                    update_alarm_state(g_button_manager->alarm_ctx, EVENT_ARM);
                    printf("System ARMED\n");
                } else {
                    printf("System already armed or in triggered state\n");
                }
            } else {
                // Switch to LOW position (0V) = DISARM
                if (g_button_manager->alarm_ctx->current_state != ALARM_STATE_DISARMED) {
                    update_alarm_state(g_button_manager->alarm_ctx, EVENT_DISARM);
                    printf("System DISARMED\n");
                } else {
                    printf("System already disarmed\n");
                }
            }
            break;
        }

        case RESET_BUTTON_PIN: {
            // Check debounce
            if (current_time - g_button_manager->reset_button.last_press_time < BUTTON_DEBOUNCE_MS) {
                return;
            }

            g_button_manager->reset_button.last_press_time = current_time;
            printf("RESET button pressed\n");
            
            // Can disarm from any state except already disarmed
            if (g_button_manager->alarm_ctx->current_state != ALARM_STATE_DISARMED) {
                update_alarm_state(g_button_manager->alarm_ctx, EVENT_RESET);
            } else {
                printf("System already disarmed\n");
            }
            break;
        }
        
        default:
            break;
    }
}
