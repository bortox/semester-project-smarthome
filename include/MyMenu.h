#pragma once

#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include "lights.h"
#include "sensors.h"

// Enum per gli input
enum MenuInput { NONE, UP, DOWN, SELECT, BACK };

class MenuItem;

// Stato globale del menu
struct MenuState {
    static MenuItem* currentPage;
};

// --- CLASSE BASE ASTRATTA ---
class MenuItem {
protected:
    const char* name;
public:
    MenuItem(const char* n) : name(n) {}
    virtual ~MenuItem() {}
    const char* getName() const { return name; }
    virtual void draw(LiquidCrystal_I2C& lcd, int row, bool isSelected) = 0;
    virtual void handleInput(MenuInput input) = 0;
    virtual void renderPage(LiquidCrystal_I2C& lcd) {}
};

// --- CLASSE PAGINA (Contenitore) ---
class MenuPage : public MenuItem {
private:
    MenuItem* items[10];
    int itemCount = 0;
    int selectedIndex = 0;
    int scrollOffset = 0;
    MenuItem* parent;

public:
    MenuPage(const char* n, MenuItem* p = nullptr) : MenuItem(n), parent(p) {}
    
    void addItem(MenuItem* item) {
        if (itemCount < 10) items[itemCount++] = item;
    }
    
    void draw(LiquidCrystal_I2C& lcd, int row, bool isSelected) override {
        lcd.setCursor(1, row);
        lcd.print(name);
    }
    
    void handleInput(MenuInput input) override {
        if (input == UP) {
            if (selectedIndex > 0) selectedIndex--;
        } else if (input == DOWN) {
            if (selectedIndex < itemCount - 1) selectedIndex++;
        } else if (input == BACK) {
            if (parent) MenuState::currentPage = parent;
        } else {
            if (itemCount > 0) {
                items[selectedIndex]->handleInput(input);
            }
        }
        if (selectedIndex < scrollOffset) scrollOffset = selectedIndex;
        if (selectedIndex >= scrollOffset + 4) scrollOffset = selectedIndex - 3;
    }
    
    void renderPage(LiquidCrystal_I2C& lcd) override {
        lcd.clear();
        for (int i = 0; i < 4; ++i) {
            int idx = scrollOffset + i;
            if (idx < itemCount) {
                items[idx]->draw(lcd, i, (idx == selectedIndex));
            }
        }
    }
};

// --- CLASSE "SUBMENU" ---
class SubMenuItem : public MenuItem {
private:
    MenuItem* targetPage;
public:
    SubMenuItem(const char* n, MenuItem* target) : MenuItem(n), targetPage(target) {}
    
    void draw(LiquidCrystal_I2C& lcd, int row, bool isSelected) override {
        lcd.setCursor(isSelected ? 0 : 1, row);
        lcd.print(isSelected ? F(">") : F(" "));
        lcd.print(name);
        lcd.setCursor(18, row);
        lcd.print(F(" >"));
    }
    
    void handleInput(MenuInput input) override {
        if (input == SELECT) {
            MenuState::currentPage = targetPage;
        }
    }
};

// --- CLASSE "TOGGLE" per ON/OFF ---
class LightToggleItem : public MenuItem {
private:
    Light* light;
public:
    LightToggleItem(Light* l) : MenuItem(l->getName()), light(l) {}
    
    void draw(LiquidCrystal_I2C& lcd, int row, bool isSelected) override {
        lcd.setCursor(isSelected ? 0 : 1, row);
        lcd.print(isSelected ? F(">") : F(" "));
        lcd.print(name);
        
        lcd.setCursor(14, row);
        if (light->getStatus()) {
            lcd.print(F("[ON] "));
        } else {
            lcd.print(F("[OFF]"));
        }
    }
    
    void handleInput(MenuInput input) override {
        if (input == SELECT) {
            light->toggle();
        }
    }
};

// --- EDITOR DI VALORI ---
template<class T>
class ValueEditorItem : public MenuItem {
private:
    T* _target;
    MenuItem* _parent;

public:
    ValueEditorItem(const char* name, T* target, MenuItem* parent) 
        : MenuItem(name), _target(target), _parent(parent) {}

    void draw(LiquidCrystal_I2C& lcd, int row, bool isSelected) override {
        lcd.setCursor(isSelected ? 0 : 1, row);
        lcd.print(isSelected ? F(">") : F(" "));
        lcd.print(name);

        String valStr = String(_target->getBrightness());
        while(valStr.length() < 3) valStr = " " + valStr;
        lcd.setCursor(13, row);
        lcd.print(valStr + F("%"));
    }

    void handleInput(MenuInput input) override {
        if (input == SELECT) {
            MenuState::currentPage = this;
            return;
        }

        if (input == UP) {
            int brightness = _target->getBrightness();
            _target->setBrightness(min(100, brightness + 5));
        }
        if (input == DOWN) {
            int brightness = _target->getBrightness();
            _target->setBrightness(max(0, brightness - 5));
        }

        if (input == BACK) {
            if (_parent) {
                MenuState::currentPage = _parent;
            }
        }
    }
    
    void renderPage(LiquidCrystal_I2C& lcd) override {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(name);
        lcd.print(F(": "));
        lcd.print(String(_target->getBrightness()) + F("%"));
        lcd.setCursor(0, 1);
        lcd.print(F("UP/DOWN - BACK"));
    }
};

// --- CLASSE "SELETTORE COLORE" ---
class ColorSelectItem : public MenuItem {
private:
    RGBLight* light;
    RGBColor color;
public:
    ColorSelectItem(const char* n, RGBLight* l, const RGBColor& c) : MenuItem(n), light(l), color(c) {}
    
    void draw(LiquidCrystal_I2C& lcd, int row, bool isSelected) override {
        lcd.setCursor(isSelected ? 0 : 1, row);
        lcd.print(isSelected ? F(">") : F(" "));
        lcd.print(name);
    }
    
    void handleInput(MenuInput input) override {
        if (input == SELECT) {
            light->setColor(color);
        }
    }
};

// --- VISUALIZZATORE SENSORI ---
template<typename T, typename U>
class SensorDisplayItem : public MenuItem {
private:
    Sensor<T>* sensor;
    const char* unit;
public:
    SensorDisplayItem(const char* n, Sensor<T>* s, const char* u = "") : MenuItem(n), sensor(s), unit(u) {}

    void draw(LiquidCrystal_I2C& lcd, int row, bool isSelected) override {
        lcd.setCursor(1, row);
        lcd.print(name);
        lcd.print(F(": "));

        String valueStr = String(sensor->getValue()) + unit;
        
        int pos = 20 - valueStr.length();
        if (pos < (int)strlen(name) + 3) pos = strlen(name) + 3;
        lcd.setCursor(pos, row);
        
        lcd.print(valueStr);
    }
    
    void handleInput(MenuInput input) override {
        // No-op
    }
};

#include "lightcontroller.h"

// --- SELEZIONE MODALITA' ---
class ModeSelectItem : public MenuItem {
private:
    OutsideController* _controller;

public:
    ModeSelectItem(const char* name, OutsideController* controller) 
        : MenuItem(name), _controller(controller) {}

    void draw(LiquidCrystal_I2C& lcd, int row, bool isSelected) override {
        lcd.setCursor(isSelected ? 0 : 1, row);
        lcd.print(isSelected ? F(">") : F(" "));
        lcd.print(name);

        const char* modeStr;
        switch (_controller->getMode()) {
            case OutsideController::MODE_OFF:          modeStr = "OFF"; break;
            case OutsideController::MODE_ON:           modeStr = "ON"; break;
            case OutsideController::MODE_AUTO_LIGHT:   modeStr = "AUTO Light"; break;
            case OutsideController::MODE_AUTO_MOTION:  modeStr = "AUTO Motion"; break;
            default:                                   modeStr = "???"; break;
        }
        
        int pos = 20 - strlen(modeStr);
        lcd.setCursor(pos, row);
        lcd.print(modeStr);
    }

    void handleInput(MenuInput input) override {
        if (input == SELECT) {
            int nextMode = (_controller->getMode() + 1) % 4;
            _controller->setMode(static_cast<OutsideController::Mode>(nextMode));
        }
    }
};