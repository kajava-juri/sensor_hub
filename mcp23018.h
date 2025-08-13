#ifndef MCP23018_H
#define MCP23018_H

#include "pico/stdlib.h"
#include "hardware/i2c.h"

// IOCON register bit structure
typedef struct {
    uint8_t INTCC : 1;    // bit 0: Interrupt Clearing Control
    uint8_t INTPOL : 1;   // bit 1: GPIO Register bit polarity
    uint8_t reserved1 : 1; // bit 2: reserved (0)
    uint8_t reserved2 : 1; // bit 3: reserved (0)
    uint8_t reserved3 : 1; // bit 4: reserved (0)
    uint8_t SEQOP : 1;    // bit 5: Sequential Operation mode
    uint8_t MIRROR : 1;   // bit 6: INT Pins Mirror bit
    uint8_t BANK : 1;     // bit 7: Controls how registers are addressed
} mcp23018_iocon_t;

// Function declarations
void mcp23018_init(i2c_inst_t *i2c, uint8_t addr);
void mcp23018_configure_iocon(i2c_inst_t *i2c, uint8_t addr, mcp23018_iocon_t *iocon);
int mcp23018_read8(uint8_t reg, uint8_t *data);
int mcp23018_store8(uint8_t reg, uint8_t data);

#endif // MCP23018_H