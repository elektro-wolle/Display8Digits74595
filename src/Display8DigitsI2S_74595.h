#pragma once

extern "C" {
#include "driver/gpio.h"
#include "soc/i2s_struct.h"
#include "rom/lldesc.h"
}

/** ESP32 has two different i2s ports. */
typedef enum {
    I2S_PORT_0,
    I2S_PORT_1,
} i2s_port_t;

/**
 * Configuration of the DIO connection to the esp GPIO for each displays.
 */
typedef struct {
    gpio_num_t map_bit_to_gpio[22];
} i2s_bus_config_t;

/**
 * Simple driver for multiple cheap 8-digit 7-segment LED displays, based on two 74HC595s (one for the segments and the other for the common-anode of the digit).
 * These displays are not chainable, therefore, each display has to be connected to a different data-line. The SCK (shift clock) and RCK (Latch enable) can be shared among the displays.
 *
 * (c) 2023 Wolfgang Jung (post@wolfgang-jung.net)
 */

class Display8DigitsI2S_74595 {
public:
    /**
     *  Initializes the displays.
     *
     *  @param port the I2S-Port to use.
     *  @param rclk GPIO-Pin for RCK aka Latch.
     *  @param sclk GPIO-Pin for SCK aka Shift-Clock.
     *  @param bus_config the configured pins for the individual displays.
     *
     */
    Display8DigitsI2S_74595(i2s_port_t port, gpio_num_t rclk, gpio_num_t sclk, i2s_bus_config_t bus_config);

    /**
     * change the content of the display `line` to the string `s`.
     * @param line the line to change.
     * @param s the string to display.
     */
    void display_text(uint8_t line, const char* s);

    /** enable the i2s transfer to the displays */
    inline void start() { i2s_start(); }

    /** Stop the i2s transfer to the displays */
    inline void stop() { i2s_stop(); }

    /** Update all lines, if they were set via `set`. */
    inline void flush()
    {
        for (int line = 0; line < MAX_LINES; line++) {
            update_i2s_buffer(line);
        }
    }

    void set(uint8_t line, uint8_t digit, char c) { segments_per_digit_buffer[line][digit] = decode_7seg(c); }

    static const uint8_t MAX_LINES = 22;
private:
    static const uint8_t NDIGITS = 8;
    static const uint8_t SEGMENTS_PER_DIGIT = 8;
    // first and last frame for rclk
    static const uint8_t FRAMES_PER_DIGIT = 2 + (2 * NDIGITS + 2 * SEGMENTS_PER_DIGIT);
    const uint8_t digit_selector[NDIGITS] = {
        0b00010000,
        0b00100000,
        0b01000000,
        0b10000000,
        0b00000001,
        0b00000010,
        0b00000100,
        0b00001000,
    };
    const uint8_t seven_seg_digits_decode[91] = {
        /* sp    !     "     #     $     %     &     '     (     )     *     +     ,     -     .     /  */
        0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x02, 0x39, 0x0F, 0x63, 0x00, 0x04, 0x40, 0x00, 0x00,
        /*  0    1     2     3     4     5     6     7     8     9     :     ;     <     =     >     ?  */
        0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* @     A     B     C     D     E     F     G     H     I     J     K     L     M     N     O  */
        0x00, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71, 0x3D, 0x76, 0x30, 0x1E, 0x75, 0x38, 0x55, 0x54, 0x5C,
        /* P     Q     R     S     T     U     V     W     X     Y     Z     [     \     ]     ^     _  */
        0x73, 0x67, 0x50, 0x6D, 0x78, 0x3E, 0x1C, 0x1D, 0x64, 0x6E, 0x5B, 0x39, 0x00, 0x0F, 0x23, 0x08,
        /* `     a     b     c     d     e     f     g     h     i     j     k     l     m     n     o  */
        0x00, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71, 0x3D, 0x76, 0x30, 0x1E, 0x75, 0x38, 0x55, 0x54, 0x5C,
        /* p     q     r     s     t     u     v     w     x     y     z                                */
        0x73, 0x67, 0x50, 0x6D, 0x78, 0x3E, 0x1C, 0x1D, 0x64, 0x6E, 0x5B
    };
    /** buffer for all digits on all connected displays. */
    uint8_t segments_per_digit_buffer[MAX_LINES][NDIGITS];    
    i2s_port_t port;
    i2s_dev_t* i2s;
    /** clock and latch */
    gpio_num_t rclk, sclk;
    /** Dma descriptor for the i2s_dma_buffer. */
    lldesc_t dma;
    /** prepared data to the displays, each of the 24 upper bits of the uint32_t represents one 
     * i2s channel, the lower two were used for rclk and sclk. */
    uint32_t i2s_dma_buffer[34 * NDIGITS];
    /** the configured pins for the individual displays. */
    i2s_bus_config_t bus_config;
    /** depends on the selected i2s port */
    int i2s_base_pin_index;

    void update_i2s_buffer(uint8_t line);
    void iomux_set_signal(gpio_num_t gpio, int signal);
    void i2s_init();
    void i2s_start();
    void i2s_reset();
    void i2s_reset_dma();
    void i2s_reset_fifo();
    void i2s_stop();
    uint8_t decode_7seg(uint8_t chr);
};
