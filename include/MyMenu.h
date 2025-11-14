#pragma once // Assicurati che questa riga sia all'inizio

#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <String.h>
#include "lights.h"
#include "sensors.h"

// Enum per gli input
enum MenuInput { NONE, UP, DOWN, SELECT, BACK };

// --- CLASSE BASE ASTRATTA ---
class MenuItem {
protected:
    String name;
public:
    MenuItem(const String& n) : name(n) {}
    virtual ~MenuItem() {}
    const String& getName() const { return name; }
    virtual void draw(LiquidCrystal_I2C& lcd, int row, bool isSelected) = 0;
    virtual MenuItem* select() = 0;
};

// --- CLASSE PAGINA ---
class MenuPage : public MenuItem {
private:
    MenuItem* items[10];
    int itemCount = 0;
    int selectedIndex = 0;
    MenuPage* parent = nullptr;
public:
    MenuPage(const String& n, MenuPage* p = nullptr) : MenuItem(n), parent(p) {}
    ~MenuPage() {
        for (int i = 0; i < itemCount; ++i) {
            delete items[i];
        }
    }
    void addItem(MenuItem* item) {
        if (itemCount < 10) {
            items[itemCount++] = item;
        }
    }
    void handleInput(MenuInput input, MenuItem*& currentActivePage); // Implementazione sotto
    void draw(LiquidCrystal_I2C& lcd, int row, bool isSelected) override;
    MenuItem* select() override;
    void renderPage(LiquidCrystal_I2C& lcd);
};

// --- WRAPPER PER LUCI ---
class LightItem : public MenuItem {
private:
    Light* light;
public:
    LightItem(Light* l) : MenuItem(String(l->getName())), light(l) {}
    void draw(LiquidCrystal_I2C& lcd, int row, bool isSelected) override;
    MenuItem* select() override;
};

// --- WRAPPER PER SENSORI (TEMPLATE) ---
// L'INTERA CLASSE E I SUOI METODI DEVONO STARE QUI, NEL .h
template <typename T>
class SensorItem : public MenuItem {
private:
    T* sensor;
    const char* unit;
public:
    SensorItem(const String& n, T* s, const char* u = "") : MenuItem(n), sensor(s), unit(u) {}

    void draw(LiquidCrystal_I2C& lcd, int row, bool isSelected) override {
        lcd.setCursor(1, row);
        lcd.print(name);
        lcd.print(":");

        String valueStr = String(sensor->getValue()) + unit;
        int padding = 16 - (1 + name.length() + 1 + valueStr.length());
        if (padding < 1) padding = 1;

        lcd.setCursor(1 + name.length() + 1 + padding, row);
        lcd.print(valueStr);
    }

    MenuItem* select() override {
        return nullptr;
    }
}; // <-- Controlla che ci sia questo punto e virgola!


// ========= IMPLEMENTAZIONI DEI METODI NON-TEMPLATE =========
// (Possono stare nel .h o in un .cpp, ma per semplicità mettiamoli qui)

inline void MenuPage::handleInput(MenuInput input, MenuItem*& currentActivePage) {
    if (input == UP) {
        if (itemCount > 0) {
            selectedIndex = (selectedIndex - 1 + itemCount) % itemCount;
        }
    } else if (input == DOWN) {
        if (itemCount > 0) {
            selectedIndex = (selectedIndex + 1) % itemCount;
        }
    } else if (input == BACK && parent != nullptr) {
        currentActivePage = parent;
    } else if (input == SELECT) {
        if (itemCount > 0 && selectedIndex < itemCount) {
            MenuItem* result = items[selectedIndex]->select();
            if (result != nullptr) {
                currentActivePage = result;
            }
        }
    }
}

inline void MenuPage::draw(LiquidCrystal_I2C& lcd, int row, bool isSelected) {
    lcd.setCursor(1, row);
    lcd.print(name);
    lcd.print(" >");
}

inline MenuItem* MenuPage::select() {
    return this;
}

inline void MenuPage::renderPage(LiquidCrystal_I2C& lcd) {
    lcd.clear();
    int start = (selectedIndex / 4) * 4;
    for (int i = 0; i < 4; i++) {
        int idx = start + i;
        if (idx < itemCount) {
            bool isCurrent = (idx == selectedIndex);
            lcd.setCursor(0, i);
            lcd.print(isCurrent ? ">" : " ");
            items[idx]->draw(lcd, i, isCurrent);
        }
    }
}

inline void LightItem::draw(LiquidCrystal_I2C& lcd, int row, bool isSelected) {
    lcd.setCursor(1, row);
    lcd.print(name);
    String statusText = light->getStatus() ? "ON" : "OFF";
    int padding = 16 - (1 + name.length() + statusText.length());
    if (padding < 1) padding = 1;
    lcd.setCursor(1 + name.length() + padding, row);
    lcd.print(statusText);
}

inline MenuItem* LightItem::select() {
    light->toggle();
    return nullptr;
}