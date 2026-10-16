#include "Display.h"

Display::Display()
    : oled(Konfig::WIDTH, Konfig::HEIGHT, &Wire, -1),
      i2cAddress(Konfig::OLED_ADRESSE) {
}

void Display::begin() {
    if (!oled.begin(SSD1306_SWITCHCAPVCC, i2cAddress)) {
        return;
    }

    oled.clearDisplay();
    oled.setTextColor(SSD1306_WHITE);
    oled.setTextSize(1);
    oled.setTextWrap(true);
    oled.display();
    oled.setCursor(0, 0);

    return;
}

void Display::clear() {
    oled.clearDisplay();
}

void Display::update() {
    oled.display();
}

void Display::setTextSize(uint8_t size) {
    oled.setTextSize(size);
}

void Display::setTextColor(uint16_t color) {
    oled.setTextColor(color);
}

void Display::setCursor(int16_t x, int16_t y) {
    oled.setCursor(x, y);
}

void Display::print(const char* text) {
    oled.clearDisplay();
    oled.print(text);
    oled.display();
}

void Display::println(const char* text) {
    oled.clearDisplay();
    oled.print(text);
    oled.display();
}

void Display::print(const String& text) {
    oled.clearDisplay();
    oled.print(text);
    oled.display();
}

void Display::print(float value) {
    oled.print(value);
}

void Display::drawPixel(int16_t x, int16_t y, uint16_t color) {
    oled.drawPixel(x, y, color);
}

void Display::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
    oled.drawLine(x0, y0, x1, y1, color);
}

void Display::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    oled.drawRect(x, y, w, h, color);
}

void Display::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    oled.fillRect(x, y, w, h, color);
}

void Display::drawCircle(int16_t x, int16_t y, int16_t radius, uint16_t color) {
    oled.drawCircle(x, y, radius, color);
}

void Display::drawHorizontalLine(int16_t x, int16_t y, int16_t width, uint16_t color) {
    oled.drawFastHLine(x, y, width, color);
}

Adafruit_SSD1306& Display::raw() {
    return oled;
}
