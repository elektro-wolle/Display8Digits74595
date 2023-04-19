#include <Arduino.h>

#include <Display8Digits74595.h>

int8_t DIO[] = {
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    11,
};

Display8Digits74595 display(2, 3, 8, DIO);

void setup()
{
    Serial.begin(115200);
}

void loop()
{

    display.displayLoop();

    while (Serial.available()) {
        static auto currentLine = String("");
        auto s = Serial.read();
        if (s != '\n' && s != '\r') {
            currentLine += (char)s;
        } else {
            // Serial.printf("Parsing %s\n", currentLine.c_str());
            if (currentLine.length() > 2 && currentLine.charAt(1) == '=') {
                auto lineToPrint = currentLine.charAt(0) - '0';
                if (lineToPrint >= 0 && lineToPrint < MAX_Display8Digits74595_DISPLAYS) {
                    display.setDisplayContent(lineToPrint, currentLine.substring(2));
                }
            }
            currentLine = String("");
        }
    }
}
