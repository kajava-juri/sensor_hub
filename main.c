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

// I2C reserves some addresses for special purposes. We exclude these from the scan.
// These are any addresses of the form 000 0xxx or 111 1xxx
bool reserved_addr(uint8_t addr) {
    return (addr & 0x78) == 0 || (addr & 0x78) == 0x78;
}

// int mcp23018_read8(uint8_t reg, uint8_t *data) {
//     // Use the 7-bit address directly - Pico SDK handles R/W bit
//     int res;
    
//     printf("DEBUG: Writing register 0x%02x to device 0x%02x\n", reg, EXPANDER_ADDR);
//     res = i2c_write_blocking(i2c_default, EXPANDER_ADDR, &reg, 1, true);  // true = keep control
//     printf("DEBUG: Write result: %d\n", res);
//     if (res < 1) return res;

//     sleep_us(50);

//     printf("DEBUG: Reading from device 0x%02x, register 0x%02x\n", EXPANDER_ADDR, reg);
//     res = i2c_read_blocking(i2c_default, EXPANDER_ADDR, data, 1, false);
//     printf("DEBUG: Read result: %d, data: 0x%02x\n", res, *data);

//     return res;
// }

// int mcp23018_store8(uint8_t reg, uint8_t data) {
//     // Use the 7-bit address directly - Pico SDK handles R/W bit
//     int res;

//     uint8_t buf[2];
//     buf[0] = reg;
//     buf[1] = data;
//     res = i2c_write_blocking(i2c_default, EXPANDER_ADDR, buf, 2, false);

//     return res;
// }

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
        .INTCC = 0,
        .INTPOL = 0,
        .reserved1 = 0,
        .reserved2 = 0,
        .reserved3 = 0,
        .SEQOP = 1,
        .MIRROR = 0,
        .BANK = 1
    };
    mcp23018_configure_iocon(i2c_default, EXPANDER_ADDR, &iocon);
    //mcp23018_store8(IOCON_BANK0, 0xa0);
    mcp23018_store8(IODIRA, 0x00);  // Set all pins as outputs

    puts("\n===Configured MCP23018 - all pins as outputs===\n");

    sleep_ms(100);

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

    puts("Set all GPIO pins HIGH - LED should light up now");

    puts("Testing read");

    uint8_t data;
    if (mcp23018_read8(GPIOA, &data) == 1) {
        printf("Read GPIOA: 0x%02x\n", data);
    } else {
        puts("Failed to read GPIOA");
    }

    // blink internal LED to indicate it is running
    while (true) {
        mcp23018_store8(GPIOA, 0x1); 
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        sleep_ms(10);
        if (mcp23018_read8(GPIOA, &data) == 1) {
            printf("Read GPIOA: 0x%02x\n", data);
        } else {
            puts("Failed to read GPIOA");
        }

        // sleep_ms(5000);
        getchar();  // Wait for user input before measuring

        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
        mcp23018_store8(GPIOA, 0x0); 
        sleep_ms(10);
        if (mcp23018_read8(GPIOA, &data) == 1) {
            printf("Read GPIOA: 0x%02x\n", data);
        } else {
            puts("Failed to read GPIOA");
        
        }
        // sleep_ms(5000);
        getchar();  // Wait for user input before measuring
    }

    return 0;
#endif
}
