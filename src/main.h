#ifndef MAIN_H
#define MAIN_H

#define DEVICE_NAME "pico_w_1"

#define EXPANDER_ADDR 0x20
#define INTERRUPT_PIN 2

#define I2C_SDA_PIN 16
#define I2C_SCL_PIN 17
#define MCP23018_RESET_PIN 19

#define I2C_BUS_FREQUENCY_khz 50  // kHz

#define IOCON_BANK0 0x0A
#define IOCON_BANK1 0x05

#define GPIOA_BANK0 0x12
#define GPIOA_BANK1 0x09

#define GPINTENA_BANK1 0x02

#define DEFVALA_BANK1 0x03

#define INTCONA_BANK1 0x04

#define INTFA_BANK1 0x07

#define INTCAPA_BANK1 0x08

void detailed_panic(const char *fmt, ...);

#endif // MAIN_H