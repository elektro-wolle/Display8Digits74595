#include "Display8DigitsI2S_74595.h"

extern "C" {
#include "driver/periph_ctrl.h"
#include "soc/i2s_reg.h"
#include <string.h>
}

Display8DigitsI2S_74595::Display8DigitsI2S_74595(i2s_port_t port, gpio_num_t rclk, gpio_num_t sclk, i2s_bus_config_t bus_config)
    : port(port)
    , rclk(rclk)
    , sclk(sclk)
    , bus_config(bus_config)
{
    i2s_init();
}

void Display8DigitsI2S_74595::display_text(uint8_t line, const char* s)
{
    if (line >= MAX_LINES) {
        return;
    }
    int length = strlen(s);
    uint8_t readIdx = 0;
    uint8_t writeIdx = 0;
    // start with blank line
    for (uint8_t digit = 0; digit < NDIGITS; digit++) {
        segments_per_digit_buffer[line][digit] = 0xff;
    }
    while (writeIdx < NDIGITS && readIdx < length) {
        char c = s[readIdx++];
        // if '.' is given, it will be added to the previous digit
        if (c == '.' && writeIdx > 0) {
            segments_per_digit_buffer[line][writeIdx - 1] &= 0x7f;
        } else if (c >= ' ' && c <= 'z') {
            segments_per_digit_buffer[line][writeIdx] = decode_7seg(c);
            writeIdx++;
        }
    }
    update_i2s_buffer(line);
}

void Display8DigitsI2S_74595::update_i2s_buffer(uint8_t line)
{
    if (line >= MAX_LINES) {
        return;
    }

    int16_t offset = 0;
    for (uint8_t digit = 0; digit < NDIGITS; digit++) {
        uint8_t val = segments_per_digit_buffer[line][digit];
        uint32_t bit_in_i2s_buffer = 1 << (10 + line);
        int idx = offset + 1;
        // first push segments of the selected digit
        for (uint8_t i = 0; i < NDIGITS; i++) {
            i2s_dma_buffer[idx] &= ~bit_in_i2s_buffer;
            i2s_dma_buffer[idx + 1] &= ~bit_in_i2s_buffer;
            if (val & (1 << (7 - i))) {
                i2s_dma_buffer[idx] |= bit_in_i2s_buffer;
                i2s_dma_buffer[idx + 1] |= bit_in_i2s_buffer;
            }
            idx += 2;
        }
        // advance to next frame (one frame per digit)
        offset += FRAMES_PER_DIGIT;
    }
    // memcpy(this->i2s_dma_buffer, temp_i2s_dma_buffer, sizeof(temp_i2s_dma_buffer));
}

void Display8DigitsI2S_74595::iomux_set_signal(gpio_num_t gpio, int signal)
{
    if (gpio < 0) {
        return;
    }
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[gpio], PIN_FUNC_GPIO);
    gpio_set_direction(gpio, GPIO_MODE_OUTPUT);
    gpio_set_pull_mode(gpio, GPIO_FLOATING);
    gpio_matrix_out(gpio, signal, false, false);
}

void Display8DigitsI2S_74595::i2s_init()
{
    if (port == I2S_PORT_0) {
        i2s = &I2S0;
        periph_module_enable(PERIPH_I2S0_MODULE);
        i2s_base_pin_index = I2S0O_DATA_OUT0_IDX;
    } else {
        i2s = &I2S1;
        periph_module_enable(PERIPH_I2S1_MODULE);
        i2s_base_pin_index = I2S1O_DATA_OUT0_IDX;
    }

    i2s_reset();
    i2s_reset_dma();
    i2s_reset_fifo();

    // Setup i2s
    i2s->conf.tx_msb_right = 1;
    i2s->conf.tx_mono = 0;
    i2s->conf.tx_short_sync = 0;
    i2s->conf.tx_msb_shift = 0;
    i2s->conf.tx_right_first = 1; // 0;//1;
    i2s->conf.tx_slave_mod = 0;

    // Enable LCD Mode
    i2s->conf2.val = 0;
    i2s->conf2.lcd_en = 1;
    i2s->conf2.lcd_tx_wrx2_en = 0; // 0 for 32 parallel output
    i2s->conf2.lcd_tx_sdx2_en = 0;

    i2s->sample_rate_conf.val = 0;
    i2s->sample_rate_conf.tx_bits_mod = 32; // using uint32_t as data
    i2s->sample_rate_conf.tx_bck_div_num = 1;

    // clock rate = 80 MHz / (div_num + (div_b/div_a)) => 400 KHz Frame Clock -> 200 KHz on SCLK
    i2s->clkm_conf.val = 0;
    i2s->clkm_conf.clka_en = 0;
    i2s->clkm_conf.clkm_div_a = 1;
    i2s->clkm_conf.clkm_div_b = 0;
    i2s->clkm_conf.clkm_div_num = 200;
    
    i2s->fifo_conf.val = 0;
    i2s->fifo_conf.tx_fifo_mod_force_en = 1;
    i2s->fifo_conf.tx_fifo_mod = 3; // 32-bit single channel data
    i2s->fifo_conf.tx_data_num = 32; // fifo length
    i2s->fifo_conf.dscr_en = 1; // fifo will use dma

    i2s->conf1.val = 0;
    i2s->conf1.tx_stop_en = 0;
    i2s->conf1.tx_pcm_bypass = 1;

    i2s->conf_chan.val = 0;
    i2s->conf_chan.tx_chan_mod = 1;

    i2s->timing.val = 0;

    // first two channels for clock signals
    iomux_set_signal(rclk, i2s_base_pin_index);
    iomux_set_signal(sclk, i2s_base_pin_index + 1);

    for (int x = 0; x < MAX_LINES; x++) {
        gpio_num_t pin = bus_config.map_bit_to_gpio[x];
        if (pin != GPIO_NUM_NC) {
            iomux_set_signal(pin, i2s_base_pin_index + x + 2);
        }
    }

    // circular i2s_dma_buffer
    dma.length = sizeof(i2s_dma_buffer);
    dma.size = sizeof(i2s_dma_buffer);
    dma.owner = 1;
    dma.sosf = 0;
    dma.buf = (uint8_t*)(&i2s_dma_buffer);
    dma.offset = 0;
    dma.empty = 0;
    dma.eof = 0;
    dma.qe.stqe_next = &dma;

    int offset = 0;
    // setting clocks
    memset(i2s_dma_buffer, 0xff, sizeof(i2s_dma_buffer));
    for (int idx = 0; idx < NDIGITS * FRAMES_PER_DIGIT; idx++) {
        i2s_dma_buffer[idx] &= ~0x300;
    }

    for (uint8_t digit = 0; digit < NDIGITS; digit++) {
        // every other frame contains the shift clock (sclk)
        for (uint8_t i = 1; i < FRAMES_PER_DIGIT - 1; i += 2) {
            i2s_dma_buffer[offset + i] |= 0x200;
        }
        // lastframe contains the output latch (rclk)
        i2s_dma_buffer[offset + FRAMES_PER_DIGIT - 1] |= 0x100;

        int idx = offset + 17;
        // the select the digit to be shown
        for (uint8_t i = 0; i < NDIGITS; i++) {
            uint32_t bits = 0;
            for (uint8_t line = 0; line < MAX_LINES; line++) {
                uint8_t val = digit_selector[digit];
                uint32_t bit = (!!(val & (1 << (7 - i)))) << (10 + line);
                bits |= bit;
            }
            i2s_dma_buffer[idx] &= 0x300;
            i2s_dma_buffer[idx] |= bits;
            i2s_dma_buffer[idx + 1] &= 0x300;
            i2s_dma_buffer[idx + 1] |= bits;
            idx += 2;
        }
        // advance to next frame (one frame per digit)
        offset += FRAMES_PER_DIGIT;
    }
    flush();
}

void Display8DigitsI2S_74595::i2s_start()
{
    i2s_reset();
    i2s->lc_conf.val = I2S_OUT_DATA_BURST_EN | I2S_OUTDSCR_BURST_EN | I2S_OUT_DATA_BURST_EN;

    // use out dma i2s_dma_buffer, linking to itself for endless loop
    i2s->out_link.addr = (uint32_t)(&dma);
    i2s->out_link.start = 1;

    i2s->int_clr.val = i2s->int_raw.val;
    i2s->int_ena.out_dscr_err = 1;
    i2s->int_ena.val = 0;
    i2s->int_ena.out_eof = 1;
    // start DMA Transfer
    i2s->conf.tx_start = 1;
}

void Display8DigitsI2S_74595::i2s_reset()
{
    // setting the bits to 1 and immediate back to 0
    const uint32_t lc_conf_reset_flags = I2S_IN_RST_M | I2S_OUT_RST_M | I2S_AHBM_RST_M | I2S_AHBM_FIFO_RST_M;
    i2s->lc_conf.val |= lc_conf_reset_flags;
    i2s->lc_conf.val &= ~lc_conf_reset_flags;

    // setting the bits to 1 and immediate back to 0
    const uint32_t conf_reset_flags = I2S_RX_RESET_M | I2S_RX_FIFO_RESET_M | I2S_TX_RESET_M | I2S_TX_FIFO_RESET_M;
    i2s->conf.val |= conf_reset_flags;
    i2s->conf.val &= ~conf_reset_flags;
}

void Display8DigitsI2S_74595::i2s_reset_dma()
{
    i2s->lc_conf.in_rst = 1;
    i2s->lc_conf.in_rst = 0;
    i2s->lc_conf.out_rst = 1;
    i2s->lc_conf.out_rst = 0;
}

void Display8DigitsI2S_74595::i2s_reset_fifo()
{
    i2s->conf.rx_fifo_reset = 1;
    i2s->conf.rx_fifo_reset = 0;
    i2s->conf.tx_fifo_reset = 1;
    i2s->conf.tx_fifo_reset = 0;
}

void Display8DigitsI2S_74595::i2s_stop()
{
    i2s_reset();
    i2s->conf.rx_start = 0;
    i2s->conf.tx_start = 0;
}

/* Invalid letters are mapped to all segments off (0x00).
Implementation uses ASCII chars from 0x20 (space) to 0x7a (z)
*/
uint8_t Display8DigitsI2S_74595::decode_7seg(uint8_t chr)
{
    if (chr > 'z' || chr <= ' ')
        return 0xff;
    return ~seven_seg_digits_decode[chr - ' '];
}
