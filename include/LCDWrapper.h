#ifndef LCD_WRAPPER_H
#define LCD_WRAPPER_H

#include <Arduino.h>
extern "C" {
    #include "lcd.h"
}

// Wrapper C++ che emula l'interfaccia di LiquidCrystal_I2C
class LiquidCrystal_I2C {
private:
    uint8_t _addr;
    uint8_t _cols;
    uint8_t _rows;
    
public:
    LiquidCrystal_I2C(uint8_t addr, uint8_t cols, uint8_t rows) 
        : _addr(addr), _cols(cols), _rows(rows) {}
    
    void init() {
        LCD_init();
    }
    
    void begin() {
        LCD_init();
    }
    
    void begin(uint8_t cols, uint8_t rows) {
        LCD_init();
    }
    
    void clear() {
        LCD_clear();
    }
    
    void home() {
        LCD_home();
    }
    
    void setCursor(uint8_t col, uint8_t row) {
        LCD_set_cursor(col, row);
    }
    
    void print(const char* str) {
        LCD_write_str(str);
    }
    
    void print(char c) {
        LCD_write_char(c);
    }
    
    void print(int value) {
        char buffer[12];
        itoa(value, buffer, 10);
        LCD_write_str(buffer);
    }
    
    void print(unsigned int value) {
        char buffer[12];
        utoa(value, buffer, 10);
        LCD_write_str(buffer);
    }
    
    void print(float value, int decimals = 2) {
        char buffer[16];
        dtostrf(value, 0, decimals, buffer);
        LCD_write_str(buffer);
    }
    
    // Support for F() macro (PROGMEM strings)
    void print(const __FlashStringHelper* str) {
        PGM_P p = reinterpret_cast<PGM_P>(str);
        char c;
        while ((c = pgm_read_byte(p++)) != 0) {
            LCD_write_char(c);
        }
    }
    
    void backlight() {
        LCD_backlight();
    }
    
    void noBacklight() {
        LCD_no_backlight();
    }
    
    void display() {
        LCD_display_on();
    }
    
    void noDisplay() {
        LCD_display_off();
    }
    
    void cursor() {
        LCD_cursor_on();
    }
    
    void noCursor() {
        LCD_cursor_off();
    }
    
    void blink() {
        LCD_blink_on();
    }
    
    void noBlink() {
        LCD_blink_off();
    }
    
    void scrollDisplayLeft() {
        LCD_scroll_display_left();
    }
    
    void scrollDisplayRight() {
        LCD_scroll_display_right();
    }
    
    void leftToRight() {
        LCD_left_to_right();
    }
    
    void rightToLeft() {
        LCD_right_to_Left();
    }
    
    void autoscroll() {
        LCD_autoscroll();
    }
    
    void noAutoscroll() {
        LCD_no_autoscroll();
    }
    
    void createChar(uint8_t location, uint8_t charmap[]) {
        LCD_create_char(location, charmap);
    }
};

#endif
