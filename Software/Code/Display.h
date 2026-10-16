#include "WString.h"
#ifndef DISPLAY_H
#define DISPLAY_H

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "Konfig.h"


class Display{
  public:
    Display();

    void begin();

    void clear();
    void update();

    void setTextSize(uint8_t size);
    void setTextColor(uint16_t color);
    void setCursor(int16_t x, int16_t y);

    void print(const char* text);
    void println(const char* text);
    void print(const String& text);
    void print(float value);

    void drawPixel(int16_t x, int16_t y,
                   uint16_t color = SSD1306_WHITE);

    void drawLine(int16_t x0, int16_t y0,
                  int16_t x1, int16_t y1,
                  uint16_t color = SSD1306_WHITE);

    void drawRect(int16_t x, int16_t y,
                  int16_t w, int16_t h,
                  uint16_t color = SSD1306_WHITE);

    void fillRect(int16_t x, int16_t y,
                  int16_t w, int16_t h,
                  uint16_t color = SSD1306_WHITE);

    void drawCircle(int16_t x, int16_t y,
                    int16_t radius,
                    uint16_t color = SSD1306_WHITE);

    void drawHorizontalLine(int16_t x, int16_t y,
                            int16_t width,
                            uint16_t color = SSD1306_WHITE);

    Adafruit_SSD1306& raw();

  private:
    Adafruit_SSD1306 oled;
    uint8_t i2cAddress;
};



#endif