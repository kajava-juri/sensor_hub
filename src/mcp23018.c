#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/i2c.h"
#include "pico/cyw43_arch.h"
#include "main.h"
#include "mcp23018.h"

#define MCP23018_IODIRA   0x00
// MCP23018 register addresses (BANK=0 mode)
#define MCP23018_IODIRB_BANK0   0x01
#define MCP23018_GPIOA_BANK0    0x12
#define MCP23018_GPIOB_BANK0    0x13
#define MCP23018_IOCON_BANK0    0x0a

// MCP23018 register addresses (BANK=1 mode)
#define MCP23018_IODIRB_BANK1   0x10
#define MCP23018_GPIOA_BANK1    0x09
#define MCP23018_GPIOB_BANK1    0x19
#define MCP23018_IOCON_BANK1    0x05

void mcp23018_init(i2c_inst_t *i2c, uint8_t addr) {
    // Initialize with default IOCON settings
    mcp23018_iocon_t iocon = {
        .INTCC = 0,
        .INTPOL = 0,
        .reserved1 = 0,
        .reserved2 = 0,
        .reserved3 = 0,
        .SEQOP = 0,
        .MIRROR = 0,
        .BANK = 0
    };

    mcp23018_configure_iocon(i2c, addr, &iocon);
}

void mcp23018_configure_iocon(i2c_inst_t *i2c, uint8_t addr, mcp23018_iocon_t *iocon) {
    int res;
    if (iocon->BANK) {
        res = mcp23018_store8(IOCON_BANK0, 0x80);
        if (res < 0) {
            printf("Failed to configure IOCON_BANK0\n");
        }
    }
    // Convert bit structure to byte value
    uint8_t iocon_value = 0;
    iocon_value |= (iocon->INTCC & 0x01) << 0;
    iocon_value |= (iocon->INTPOL & 0x01) << 1;
    iocon_value |= (iocon->SEQOP & 0x01) << 5;
    iocon_value |= (iocon->MIRROR & 0x01) << 6;
    iocon_value |= (iocon->BANK & 0x01) << 7;
    
    
    res = mcp23018_store8(IOCON_BANK1, iocon_value);
    if (res < 0) {
        printf("Failed to configure IOCON\n");
    }
}

int mcp23018_read8(uint8_t reg, uint8_t *data) {
    // Use the 7-bit address directly - Pico SDK handles R/W bit
    int res;
    
    //printf("DEBUG: Writing register 0x%02x to device 0x%02x\n", reg, EXPANDER_ADDR);
    // Use explicit timeout - 10ms should be more than enough for I2C at 50kHz
    res = i2c_write_timeout_us(i2c_default, EXPANDER_ADDR, &reg, 1, true, 10000);  // 10ms timeout
    //printf("DEBUG: Write result: %d\n", res);
    if (res < 1) {
        printf("DEBUG: Write error code: %d\n", res);
        return res;
    }

    //printf("DEBUG: Reading from device 0x%02x, register 0x%02x\n", EXPANDER_ADDR, reg);
    // Use explicit timeout - 10ms should be more than enough
    res = i2c_read_timeout_us(i2c_default, EXPANDER_ADDR, data, 1, false, 10000);  // 10ms timeout
    //printf("DEBUG: Read result: %d, data: 0x%02x\n", res, *data);
    
    if (res < 1) {
        printf("DEBUG: Read error code: %d\n", res);
    }
    
    return res;
}

int mcp23018_store8(uint8_t reg, uint8_t data) {
    // Use the 7-bit address directly - Pico SDK handles R/W bit
    int res;

    uint8_t buf[2];
    buf[0] = reg;
    buf[1] = data;
    res = i2c_write_blocking(i2c_default, EXPANDER_ADDR, buf, 2, false);

    return res;
}