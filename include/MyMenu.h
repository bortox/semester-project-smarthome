#pragma once

#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <String.h>
#include "lights.h"
#include "sensors.h"

// Enum per gli input
enum MenuInput { NONE, UP, DOWN, LEFT, RIGHT, SELECT, BACK };

class MenuItem; // Forward declaration

// Stato globale del menu
struct MenuState {
    static MenuItem* currentPage;
};

// --- CLASSE BASE ASTRATTA ---
class MenuItem {
protected:
    String name;
public:
    MenuItem(const String& n) : name(n) {}
    virtual ~MenuItem() {}
    const String& getName() const { return name; }
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
    MenuPage(const String& n, MenuItem* p = nullptr) : MenuItem(n), parent(p) {}
    ~MenuPage() {
        for (int i = 0; i < itemCount; ++i) delete items[i];
    }
    void addItem(MenuItem* item) {
        if (itemCount < 10) items[itemCount++] = item;
    }
    void draw(LiquidCrystal_I2C& lcd, int row, bool isSelected) override;
    void handleInput(MenuInput input) override;
    void renderPage(LiquidCrystal_I2C& lcd) override;
};

// --- CLASSE "SUBMENU" ---
class SubMenuItem : public MenuItem {
private:
    MenuItem* targetPage;
public:
    SubMenuItem(const String& n, MenuItem* target) : MenuItem(n), targetPage(target) {}
    void draw(LiquidCrystal_I2C& lcd, int row, bool isSelected) override;
    void handleInput(MenuInput input) override;
};

// --- CLASSE "TOGGLE" per ON/OFF ---
class LightToggleItem : public MenuItem {
private:
    Light* light;
public:
    LightToggleItem(Light* l) : MenuItem(String(l->getName())), light(l) {}
    void draw(LiquidCrystal_I2C& lcd, int row, bool isSelected) override;
    void handleInput(MenuInput input) override;
};

// --- CLASSE "EDITOR DI VALORE" per DimmableLight ---
class DimmerItem : public MenuItem {
private:
    DimmableLight* light;
public:
    DimmerItem(const String& n, DimmableLight* l) : MenuItem(n), light(l) {}
    void draw(LiquidCrystal_I2C& lcd, int row, bool isSelected) override;
    void handleInput(MenuInput input) override;
};

// --- CLASSE "ADATTATORE" per il Dimmer della RGBLight ---
class RGBLightDimmerItem : public MenuItem {
private:
    RGBLight* light;
public:
    RGBLightDimmerItem(const String& n, RGBLight* l) : MenuItem(n), light(l) {}
    void draw(LiquidCrystal_I2C& lcd, int row, bool isSelected) override;
    void handleInput(MenuInput input) override;
};

// --- CLASSE "SELETTORE COLORE" ---
class ColorSelectItem : public MenuItem {
private:
    RGBLight* light;
    RGBColor color;
public:
    ColorSelectItem(const String& n, RGBLight* l, const RGBColor& c) : MenuItem(n), light(l), color(c) {}
    void draw(LiquidCrystal_I2C& lcd, int row, bool isSelected) override;
    void handleInput(MenuInput input) override;
};


// ========= IMPLEMENTAZIONI DEI METODI =========

// --- MenuPage ---
inline void MenuPage::draw(LiquidCrystal_I2C& lcd, int row, bool isSelected) {
    // Questo metodo è per quando una pagina è un item in un'altra pagina (raro)
    lcd.setCursor(1, row);
    lcd.print(name);
}
inline void MenuPage::handleInput(MenuInput input) {
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
inline void MenuPage::renderPage(LiquidCrystal_I2C& lcd) {
    lcd.clear();
    for (int i = 0; i < 4; ++i) {
        int idx = scrollOffset + i;
        if (idx < itemCount) {
            items[idx]->draw(lcd, i, (idx == selectedIndex));
        }
    }
}

// --- SubMenuItem ---
inline void SubMenuItem::draw(LiquidCrystal_I2C& lcd, int row, bool isSelected) {
    lcd.setCursor(isSelected ? 0 : 1, row);
    lcd.print(isSelected ? ">" : " ");
    lcd.print(name);
    lcd.setCursor(18, row);
    lcd.print(" >");
}
inline void SubMenuItem::handleInput(MenuInput input) {
    if (input == SELECT) {
        MenuState::currentPage = targetPage;
    }
}

// --- LightToggleItem ---
inline void LightToggleItem::draw(LiquidCrystal_I2C& lcd, int row, bool isSelected) {
    lcd.setCursor(isSelected ? 0 : 1, row);
    lcd.print(isSelected ? ">" : " ");
    lcd.print(name);
    String status = light->getStatus() ? "[ON] " : "[OFF]";
    lcd.setCursor(14, row);
    lcd.print(status);
}
inline void LightToggleItem::handleInput(MenuInput input) {
    if (input == SELECT) {
        light->toggle();
    }
}

// --- DimmerItem ---
inline void DimmerItem::draw(LiquidCrystal_I2C& lcd, int row, bool isSelected) {
    lcd.setCursor(isSelected ? 0 : 1, row);
    lcd.print(isSelected ? ">" : " ");
    lcd.print(name);
    String valStr = String(light->getBrightness());
    while(valStr.length() < 3) valStr = " " + valStr;
    lcd.setCursor(14, row);
    lcd.print(valStr + "%");
}
inline void DimmerItem::handleInput(MenuInput input) {
    int brightness = light->getBrightness();
    if (input == RIGHT) {
        brightness = min(100, brightness + 5);
        light->setBrightness(brightness);
    } else if (input == LEFT) {
        brightness = max(0, brightness - 5);
        light->setBrightness(brightness);
    }
}

// --- RGBLightDimmerItem ---
inline void RGBLightDimmerItem::draw(LiquidCrystal_I2C& lcd, int row, bool isSelected) {
    lcd.setCursor(isSelected ? 0 : 1, row);
    lcd.print(isSelected ? ">" : " ");
    lcd.print(name);
    String valStr = String(light->getBrightness());
    while(valStr.length() < 3) valStr = " " + valStr;
    lcd.setCursor(14, row);
    lcd.print(valStr + "%");
}
inline void RGBLightDimmerItem::handleInput(MenuInput input) {
    int brightness = light->getBrightness();
    if (input == RIGHT) {
        brightness = min(100, brightness + 5);
        light->setBrightness(brightness);
    } else if (input == LEFT) {
        brightness = max(0, brightness - 5);
        light->setBrightness(brightness);
    }
}

// --- ColorSelectItem ---
inline void ColorSelectItem::draw(LiquidCrystal_I2C& lcd, int row, bool isSelected) {
    lcd.setCursor(isSelected ? 0 : 1, row);
    lcd.print(isSelected ? ">" : " ");
    lcd.print(name);
}
inline void ColorSelectItem::handleInput(MenuInput input) {
    if (input == SELECT) {
        light->setColor(color);
    }
}

// ... (dopo la classe ColorSelectItem)

// --- CLASSE PER VISUALIZZARE UN VALORE DA UN SENSORE ---
template<typename T, typename U>
class SensorDisplayItem : public MenuItem {
private:
    Sensor<T>* sensor;
    const char* unit;
public:
    SensorDisplayItem(const String& n, Sensor<T>* s, const char* u = "") : MenuItem(n), sensor(s), unit(u) {}

    void draw(LiquidCrystal_I2C& lcd, int row, bool isSelected) override {
        lcd.setCursor(1, row);
        lcd.print(name);
        lcd.print(": ");

        // Leggi il valore FRESCO del sensore ogni volta che disegni
        String valueStr = String(sensor->getValue()) + unit;
        
        // Calcola la posizione per allineare a destra
        int pos = 20 - valueStr.length();
        if (pos < name.length() + 3) pos = name.length() + 3; // Evita sovrascrizioni
        lcd.setCursor(pos, row);
        
        lcd.print(valueStr);
    }
    
    // Selezionare un sensore non fa nulla
    void handleInput(MenuInput input) override {
        // No-op
    }
};