/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

// Sweep through all 7-bit I2C addresses, to see if any slaves are present on
// the I2C bus. Print out a table that looks like this:
//
// I2C Bus Scan
//    0 1 2 3 4 5 6 7 8 9 A B C D E F
// 00 . . . . . . . . . . . . . . . .
// 10 . . @ . . . . . . . . . . . . .
// 20 . . . . . . . . . . . . . . . .
// 30 . . . . @ . . . . . . . . . . .
// 40 . . . . . . . . . . . . . . . .
// 50 . . . . . . . . . . . . . . . .
// 60 . . . . . . . . . . . . . . . .
// 70 . . . . . . . . . . . . . . . .
// E.g. if addresses 0x12 and 0x34 were acknowledged.

#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "pico/cyw43_arch.h"
#include "main.h"
#include "mcp23018.h"

#define BANK0_IODIRA 0x00
#define BANK0_IODIRB 0x01
#define BANK0_IPOLA  0x02
#define BANK0_IPOLB  0x03
#define BANK0_GPINTENA 0x04
#define BANK0_GPINTENB 0x05

#define IODIRA 0x00
#define GPIOA 0x09

static char event_str[128];
volatile bool mcp23018_interrupt_pending = false;

void gpio_event_string(char *buf, uint32_t events);

// I2C reserves some addresses for special purposes. We exclude these from the scan.
// These are any addresses of the form 000 0xxx or 111 1xxx
bool reserved_addr(uint8_t addr) {
    return (addr & 0x78) == 0 || (addr & 0x78) == 0x78;
}

void gpio_callback(uint gpio, uint32_t events) {
    gpio_event_string(event_str, events);
    printf("GPIO %d %s\n", gpio, event_str);
    if (gpio == INTERRUPT_PIN) {
        printf("\\Door sensor interrupt being handled.../\n\n");
        mcp23018_interrupt_pending = true;
    }
}

void bus_scan() {
    printf("\nI2C Bus Scan\n");
    printf("   0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\n");

    for (int addr = 0; addr < (1 << 7); ++addr) {
        if (addr % 16 == 0) {
            printf("%02x ", addr);
        }

        // Perform a 1-byte dummy read from the probe address. If a slave
        // acknowledges this address, the function returns the number of bytes
        // transferred. If the address byte is ignored, the function returns
        // -1.

        // Skip over any reserved addresses.
        int ret;
        uint8_t rxdata;
        if (reserved_addr(addr))
            ret = PICO_ERROR_GENERIC;
        else
            ret = i2c_read_blocking(i2c_default, addr, &rxdata, 1, false);

        printf(ret < 0 ? "." : "@");
        printf(addr % 16 == 15 ? "\n" : "  ");
    }
    printf("Done.\n");
}

int main() {
    // Enable UART so we can print status output
    stdio_init_all();
    gpio_init(INTERRUPT_PIN);
    gpio_set_dir(INTERRUPT_PIN, GPIO_IN);
    gpio_pull_up(INTERRUPT_PIN);  // Pull up since INTA is open-drain, active low
    gpio_set_irq_enabled_with_callback(INTERRUPT_PIN, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    if (cyw43_arch_init()) {
        printf("Wi-Fi init failed");
        return -1;
    }
#if !defined(i2c_default) || !defined(I2C_SDA_PIN) || !defined(I2C_SCL_PIN)
#warning i2c/bus_scan example requires a board with I2C pins
    puts("I2C pins were not defined");
#else
    // This example will use I2C0 on the default SDA and SCL pins (GP4, GP5 on a Pico)
    i2c_init(i2c_default, 50 * 1000);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);
    // Make the I2C pins available to picotool
    bi_decl(bi_2pins_with_func(I2C_SDA_PIN, I2C_SCL_PIN, GPIO_FUNC_I2C));

    sleep_ms(1000);  // Allow time for I2C to stabilize

    puts("===========================\nQuick device check...");
    uint8_t test_byte;
    int result;
    int max_attempts = 5;
    int attempts_count = max_attempts;
    while(attempts_count--) {
        result = i2c_read_blocking(i2c_default, EXPANDER_ADDR, &test_byte, 1, false);
        if (result < 0) {
            printf("ERROR: MCP23018 not responding at expected address! %02x\n", EXPANDER_ADDR);
            if(attempts_count % max_attempts == 0) bus_scan();
        } else {
            printf("Device found at 0x%02x\n", EXPANDER_ADDR);
            break;  // Device found, exit loop
        }
    }
    if (!attempts_count) return -1;

    puts("////////////////\n\nReading IODIR register");
    uint8_t iodir_data;
    if (mcp23018_read8(IODIRA, &iodir_data) == 1) {
        printf("Read IODIR: 0x%02x\n", iodir_data);
    } else {
        puts("Failed to read IODIR");
    }

    mcp23018_iocon_t iocon = {
        .INTCC = 1,
        .INTPOL = 0,  // Change to 0: INTA is active LOW (open-drain)
        .reserved1 = 0,
        .reserved2 = 0,
        .reserved3 = 0,
        .SEQOP = 1,
        .MIRROR = 0,
        .BANK = 1
    };
    mcp23018_configure_iocon(i2c_default, EXPANDER_ADDR, &iocon);
    //mcp23018_store8(IOCON_BANK0, 0xa0);
    mcp23018_store8(IODIRA, 0xfe);  // Set all pins as inputs except GP0

    puts("\n===Configured MCP23018 - all pins as inputs except GP0===\n");

    sleep_ms(10);

    mcp23018_store8(GPINTENA_BANK1, 0x80);  // Enable interrupt on GPA7
    mcp23018_store8(INTCONA_BANK1, 0x00);   // Compare against previous value (edge detection)
    // Remove DEFVALA setting since we're using previous value mode

    puts("Verifying...");

    uint8_t new_iocon_data;
    if (mcp23018_read8(IOCON_BANK1, &new_iocon_data) == 1) {
        printf("Read IOCON: 0x%02x\n", new_iocon_data);
    } else {
        puts("Failed to read IOCON");
    }

    uint8_t new_iodir_data;
    if (mcp23018_read8(IODIRA, &new_iodir_data) == 1) {
        printf("Read IODIR: 0x%02x\n", new_iodir_data);
    } else {
        puts("Failed to read IODIR");
    }
    puts("\n");

    sleep_ms(100);

    mcp23018_store8(GPIOA, 0x1);  // Set all pins HIGH

    sleep_ms(100);

    puts("Testing read");

    uint8_t data;
    if (mcp23018_read8(GPIOA, &data) == 1) {
        printf("Read GPIOA: 0x%02x\n", data);
    } else {
        puts("Failed to read GPIOA");
    }

    // Initialize rate limiter variables
    uint32_t last_print_time = 0;
    uint32_t current_time = 0;

    // blink internal LED to indicate it is running
    while (true) {

        if (mcp23018_interrupt_pending) {
            mcp23018_interrupt_pending = false;
            // Handle the interrupt from the MCP23018
            uint8_t intf;
            uint8_t intcap;
            if (mcp23018_read8(INTFA_BANK1, &intf) == 1) {
                printf("Interrupt on GPIOA: 0x%02x\n", intf);
                sleep_ms(10);  // Allow time for the interrupt to settle
                if (mcp23018_read8(INTCAPA_BANK1, &intcap) == 1) {
                    printf("Interrupt capture on GPIOA: 0x%02x\n", intcap);
                } else {
                    puts("Failed to read interrupt capture");
                }
            } else {
                puts("Failed to read interrupt flags");
            }

        }

        current_time = to_ms_since_boot(get_absolute_time());
        
        if (mcp23018_read8(GPIOA, &data) != 1) {
            if (current_time - last_print_time >= 500) {
                puts("Failed to read GPIOA");
                last_print_time = current_time;
            }
        }
        
        // // read gpio 7
        // if (data & 0x80) {
        //     // GPIO 7 is high
        //     mcp23018_store8(GPIOA, 0x0); 
        //     cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
        // } else {
        //     // GPIO 7 is low
        //     mcp23018_store8(GPIOA, 0x1);
        //     cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        // }

        // Print GPIO pin 2 status every 1000ms
        if (current_time - last_print_time >= 1000) {
            bool pin2_state = gpio_get(2);
            printf("Pico GPIO pin 2 status: %s\n", pin2_state ? "HIGH" : "LOW");
            last_print_time = current_time;
        }

        // if(!mcp23018_interrupt_pending && gpio_get(2)) {
        //     uint8_t intcap;
        //     // If no interrupt is pending and GPIO 2 is high, attempt to clear interrupt
        //     if (mcp23018_read8(INTCAPA_BANK1, &intcap) == 1) {
        //         printf("!!!!!! Interrupt capture on GPIOA: 0x%02x\n", intcap);
        //     } else {
        //         puts("Failed to read interrupt capture");
        //     }
        // }
        
        sleep_ms(50);
    }

    return 0;
#endif
}

static const char *gpio_irq_str[] = {
        "LEVEL_LOW",  // 0x1
        "LEVEL_HIGH", // 0x2
        "EDGE_FALL",  // 0x4
        "EDGE_RISE"   // 0x8
};

void gpio_event_string(char *buf, uint32_t events) {
    for (uint i = 0; i < 4; i++) {
        uint mask = (1 << i);
        if (events & mask) {
            // Copy this event string into the user string
            const char *event_str = gpio_irq_str[i];
            while (*event_str != '\0') {
                *buf++ = *event_str++;
            }
            events &= ~mask;

            // If more events add ", "
            if (events) {
                *buf++ = ',';
                *buf++ = ' ';
            }
        }
    }
    *buf++ = '\0';
}