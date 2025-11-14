#pragma once
#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include "lights.h"   // Le tue classi originali
#include "sensors.h"  // Le tue classi originali

// Enum per gli input (dal rotary encoder o tastiera)
enum MenuInput { NONE, UP, DOWN, SELECT, BACK };

// --- CLASSE BASE ASTRATTA ---
class MenuItem {
protected:
    const char* name;
public:
    MenuItem(const char* n) : name(n) {}
    virtual ~MenuItem() {}

    const char* getName() { return name; }

    // Metodi virtuali che le sottoclassi DEVONO implementare
    // draw: disegna la riga (es. "> Luce Cucina [ON]")
    virtual void draw(LiquidCrystal_I2C& lcd, int row, bool isSelected) = 0;
    
    // select: cosa succede quando premo il pulsante? 
    // Ritorna un puntatore a una nuova pagina se si cambia menu, o nullptr se si resta qui.
    virtual MenuItem* select() = 0; 
};

// --- CLASSE PAGINA (Contenitore di item) ---
class MenuPage : public MenuItem {
private:
    MenuItem* items[10]; // Array fisso per semplicità (o usa vector se ti senti audace)
    int itemCount = 0;
    int selectedIndex = 0;
    MenuPage* parent = nullptr; // Per tornare indietro

public:
    MenuPage(const char* n, MenuPage* p = nullptr) : MenuItem(n), parent(p) {}

    void addItem(MenuItem* item) {
        if (itemCount < 10) {
            items[itemCount++] = item;
        }
    }

    // Gestione Navigazione
    void handleInput(MenuInput input, MenuItem*& currentActivePage) {
        if (input == UP) {
            selectedIndex--;
            if (selectedIndex < 0) selectedIndex = itemCount - 1;
        } else if (input == DOWN) {
            selectedIndex++;
            if (selectedIndex >= itemCount) selectedIndex = 0;
        } else if (input == BACK && parent != nullptr) {
            currentActivePage = parent; // Torna al genitore
        } else if (input == SELECT) {
            MenuItem* result = items[selectedIndex]->select();
            if (result != nullptr) {
                // Se l'item selezionato ritorna una pagina (es. è un sottomenu), cambiamo pagina attiva
                currentActivePage = result;
            }
        }
    }

    // Implementazione di draw per la Pagina intera
    void draw(LiquidCrystal_I2C& lcd, int row, bool isSelected) override {
        // Questo metodo serve se la pagina è vista come una riga in un menu padre
        lcd.setCursor(1, row);
        lcd.print(name);
        lcd.print(" >");
    }

    // Selezionare una pagina (da un menu padre) significa entrarci
    MenuItem* select() override {
        return this;
    }

    // Metodo specifico per disegnare il contenuto DELLA pagina
    void renderPage(LiquidCrystal_I2C& lcd) {
        lcd.clear();
        // Titolo in alto (opzionale, ruba una riga)
        // lcd.setCursor(0,0); lcd.print(name); 
        
        // Logica di rendering semplificata (mostra 4 righe a partire dall'indice corrente)
        // Per uno scrolling vero servirebbe una variabile 'scrollOffset'
        int start = (selectedIndex / 4) * 4; // Paginazione a blocchi di 4
        
        for (int i = 0; i < 4; i++) {
            int idx = start + i;
            if (idx < itemCount) {
                bool isCurrent = (idx == selectedIndex);
                lcd.setCursor(0, i);
                lcd.print(isCurrent ? ">" : " "); // Cursore
                items[idx]->draw(lcd, i, isCurrent);
            }
        }
    }
};

// --- WRAPPER PER LE LUCI (Il cuore della tua richiesta) ---
class LightItem : public MenuItem {
private:
    Light* light; // Riferimento al tuo oggetto originale (senza toccare la classe)

public:
    LightItem(Light* l) : MenuItem(l->getName()), light(l) {}

    void draw(LiquidCrystal_I2C& lcd, int row, bool isSelected) override {
        lcd.setCursor(1, row);
        lcd.print(name);
        lcd.setCursor(16, row); // Allinea a destra
        lcd.print(light->getStatus() ? "ON " : "OFF");
    }

    MenuItem* select() override {
        // Qui avviene la magia: modifichiamo l'oggetto originale
        light->setStatus(!light->getStatus());
        return nullptr; // Rimaniamo in questa pagina
    }
};

// --- WRAPPER PER I SENSORI ---
template <typename T>
class SensorItem : public MenuItem {
private:
    T* sensor;
    const char* unit;

public:
    SensorItem(const char* n, T* s, const char* u = "") : MenuItem(n), sensor(s), unit(u) {}

    void draw(LiquidCrystal_I2C& lcd, int row, bool isSelected) override {
        lcd.setCursor(1, row);
        lcd.print(name); // Es: "Temp Ext"
        lcd.print(":");
        lcd.print(sensor->getValue()); // Polimorfismo del sensore originale
        lcd.print(unit);
    }

    MenuItem* select() override {
        // Cliccare su un sensore potrebbe forzare una lettura o non fare nulla
        return nullptr; 
    }
};