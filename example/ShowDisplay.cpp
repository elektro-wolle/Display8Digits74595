#include <Arduino.h>

#include <Display8DigitsI2S_74595.h>

i2s_bus_config_t cfg = {
    .map_bit_to_gpio = {
        GPIO_NUM_14,
        GPIO_NUM_27,
        GPIO_NUM_NC,
        GPIO_NUM_NC,
        GPIO_NUM_NC,
        GPIO_NUM_NC,
        GPIO_NUM_NC,
        GPIO_NUM_NC,

        GPIO_NUM_NC,
        GPIO_NUM_NC,
        GPIO_NUM_NC,
        GPIO_NUM_NC,
        GPIO_NUM_NC,
        GPIO_NUM_NC,
        GPIO_NUM_NC,
        GPIO_NUM_NC,

        GPIO_NUM_NC,
        GPIO_NUM_NC,
        GPIO_NUM_NC,
        GPIO_NUM_NC,
        GPIO_NUM_NC,
        GPIO_NUM_NC,
    }
};

Display8DigitsI2S_74595 display(I2S_PORT_0, GPIO_NUM_13, GPIO_NUM_12, cfg);

void setup()
{
    Serial.begin(115200);
    display.start();
    display.display_text(0, "Hello");
    display.display_text(1, "World");
}

void loop()
{

    while (Serial.available()) {
        static auto currentLine = String("");
        auto s = Serial.read();
        if (s != '\n' && s != '\r') {
            currentLine += (char)s;
        } else {
            // Serial.printf("Parsing %s\n", currentLine.c_str());
            if (currentLine.length() > 2 && currentLine.charAt(1) == '=') {
                auto lineToPrint = currentLine.charAt(0) - '0';
                if (lineToPrint >= 0 && lineToPrint < Display8DigitsI2S_74595::MAX_LINES) {
                    display.display_text(lineToPrint, currentLine.substring(2).c_str());
                }
            }
            currentLine = String("");
        }
    }
}
