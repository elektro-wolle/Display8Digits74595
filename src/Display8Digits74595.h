#pragma once

#ifndef Display8Digits74595_h
#define Display8Digits74595_h

#include <Arduino.h>

#ifndef MAX_Display8Digits74595_DISPLAYS
#define MAX_Display8Digits74595_DISPLAYS 8
#endif

/**
 * Simple driver for multiple cheap 8-digit 7-segment LED displays, based on two 74HC595s (one for the segments and the other for the common-anode of the digit).
 * These displays are not chainable, therefore, each display has to be connected to a different data-line. The SCK (shift clock) and RCK (Latch enable) can be shared among the displays.
 * 
 * (c) 2023 Wolfgang Jung (post@wolfgang-jung.net)
 */
class Display8Digits74595 {
public:
    /**
     *  Initializes the displays.
     *
     *  @param rclkPin GPIO-Pin for RCK aka Latch.
     *  @param sclkPin GPIO-Pin for SCK aka Shift-Clock.
     *  @param numberOfDisplays, size of the dioPin-Array.
     *  @param dioPins array of size numberOfDisplays with the GPIO-Pins for DIO of each display.
     *
     */
    Display8Digits74595(const uint8_t rclkPin, const uint8_t sclkPin, const uint8_t numberOfDisplays, const int8_t dioPins[])
        : rclk(rclkPin)
        , sclk(sclkPin)
        , numberOfDiplayLines(numberOfDisplays)
    {
        memset(dio, -1, MAX_Display8Digits74595_DISPLAYS);
        if (numberOfDisplays <= MAX_Display8Digits74595_DISPLAYS) {
            memcpy(dio, dioPins, numberOfDiplayLines);

            for (uint8_t line = 0; line < numberOfDiplayLines; line++) {
                for (uint8_t digit = 0; digit < 8; digit++) {
                    segmentBuffer[line][digit] = decode_7seg('A' + digit);
                }
                if (dio[line] >= 0) {
                    pinMode(dio[line], OUTPUT);
                }
            }
            pinMode(rclk, OUTPUT);
            pinMode(sclk, OUTPUT);
        }
    }

    /**
     * set the content of the display connected to the mapped line.
     * @param line the display behind dioPin[line].
     * @param text the String to display. Single dots are appended to the previous digit.
     */
    void setDisplayContent(uint8_t line, String text)
    {
        if (line >= numberOfDiplayLines) {
            return;
        }
        uint8_t readIdx = 0;
        uint8_t writeIdx = 0;
        // Serial.printf("Displaying %s in line %d\n", s.c_str(), line);
        for (uint8_t digit = 0; digit < NDIGITS; digit++) {
            segmentBuffer[line][digit] = 0xff;
        }
        while (writeIdx < 8 && readIdx < text.length()) {
            char c = text.charAt(readIdx++);
            if (c == '.' && writeIdx > 0) {
                segmentBuffer[line][writeIdx - 1] &= 0x7f;
            } else if (c >= ' ' && c <= 'z') {
                segmentBuffer[line][writeIdx] = decode_7seg(c);
                writeIdx++;
            }
        }
    }

    /** Blanks the display. */
    void clear()
    {
        for (uint8_t line = 0; line < numberOfDiplayLines; line++) {
            for (uint8_t digit = 0; digit < 8; digit++) {
                segmentBuffer[line][digit] = 0xff;
            }
        }
    }

    /** run this at least 200 times per second to avoid flicker. */
    void displayLoop()
    {
        // disable Latch
        digitalWrite(rclk, LOW);
        // First push the segments of each display
        for (uint8_t i = 0; i < NDIGITS; i++) {
            uint8_t bitMask = 1 << (7 - i);
            for (auto line = 0; line < numberOfDiplayLines; line++) {
                auto val = segmentBuffer[line][digitToDisplay];
                auto dataPin = dio[line];
                if (dataPin >= 0) {
                    digitalWrite(dataPin, !!(val & bitMask));
                }
            }
            digitalWrite(sclk, HIGH);
            digitalWrite(sclk, LOW);
        }
        // Then the common-anode
        for (uint8_t i = 0; i < NDIGITS; i++) {
            uint8_t bitMask = 1 << (7 - i);
            for (auto line = 0; line < numberOfDiplayLines; line++) {
                auto dataPin = dio[line];
                auto val = commonAnodeSelector[digitToDisplay];
                if (dataPin >= 0) {
                    digitalWrite(dataPin, !!(val & bitMask));
                }
            }
            digitalWrite(sclk, HIGH);
            digitalWrite(sclk, LOW);
        }

        digitalWrite(rclk, HIGH);
        digitToDisplay++;
        if (digitToDisplay == 8) {
            digitToDisplay = 0;
        }
    }

private:
    static const uint8_t NDIGITS = 8;
    uint8_t rclk;
    uint8_t sclk;
    uint8_t numberOfDiplayLines;
    int8_t dio[MAX_Display8Digits74595_DISPLAYS];

    const uint8_t commonAnodeSelector[NDIGITS] = {
        0b00010000,
        0b00100000,
        0b01000000,
        0b10000000,
        0b00000001,
        0b00000010,
        0b00000100,
        0b00001000,
    };

    /** Ascii table for 7-segment display, based upon https://en.wikichip.org/wiki/seven-segment_display/representing_letters */
    const uint8_t seven_seg_digits_decode[91] = {
        // 0x20 sp  0x21 !  0x22 "  0x23 #  0x24 $  0x25 %  0x26 &  0x27 '
        0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x01,
        // 0x28 (   0x29 )  0x2a *  0x2b +  0x2c ,  0x2d -  0x2e .  0x2f /
        0x0, 0x0, 0x0, 0x0, 0x02, 0x40, 0x0, 0x00,
        /*  0     1     2     3     4     5     6     7     8     9     :     ;     */
        0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F, 0x00, 0x00,
        /*  <     =     >     ?     @     A     B     C     D     E     F     G     */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71, 0x3D,
        /*  H     I     J     K     L     M     N     O     P     Q     R     S     */
        0x76, 0x30, 0x1E, 0x75, 0x38, 0x55, 0x54, 0x5C, 0x73, 0x67, 0x50, 0x6D,
        /*  T     U     V     W     X     Y     Z     [     \     ]     ^     _     */
        0x78, 0x3E, 0x1C, 0x1D, 0x64, 0x6E, 0x5B, 0x00, 0x00, 0x00, 0x00, 0x00,
        /*  `     a     b     c     d     e     f     g     h     i     j     k     */
        0x00, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71, 0x3D, 0x76, 0x30, 0x1E, 0x75,
        /*  l     m     n     o     p     q     r     s     t     u     v     w     */
        0x38, 0x55, 0x54, 0x5C, 0x73, 0x67, 0x50, 0x6D, 0x78, 0x3E, 0x1C, 0x1D,
        /*  x     y     z     */
        0x64, 0x6E, 0x5B
    };

    /* Invalid letters are mapped to all segments off (0x00). */
    uint8_t decode_7seg(uint8_t chr)
    {
        /* Implementation uses ASCII */
        if (chr > 'z' || chr <= ' ')
            return 0xff;
        return ~seven_seg_digits_decode[chr - ' '];
    }

    uint8_t segmentBuffer[MAX_Display8Digits74595_DISPLAYS][NDIGITS];
    int digitToDisplay = 0;
};

#endif