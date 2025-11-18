#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include "config.h"
#include "MemoryMonitor.h"

LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLS, LCD_ROWS);
MenuItem* MenuState::currentPage = nullptr;
bool needsRedraw = true;

void setup() {
    Serial.begin(9600);
    while(!Serial);
    
    Serial.println(F("\n=== MEMORY OPTIMIZATION TEST ==="));
    
    // 1. Report memoria INIZIALE
    Serial.println(F("\n[1] Memoria prima inizializzazione:"));
    printMemoryReport();
    
    // 2. Inizializza LCD
    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print(F("Memory Test"));
    lcd.setCursor(0, 1);
    lcd.print(F("Free: "));
    lcd.print(getFreeMemory());
    
    // 3. Inizializza sensore temperatura
    i2c_init();
    tempSensor.begin();
    
    // 4. Report memoria DOPO hardware
    Serial.println(F("\n[2] Memoria dopo hardware init:"));
    printMemoryReport();
    
    // 5. Costruisci menu STATICO
    MenuState::currentPage = buildMenu();
    
    // 6. Report memoria FINALE
    Serial.println(F("\n[3] Memoria dopo menu construction:"));
    printMemoryReport();
    
    Serial.println(F("\n=== SYSTEM READY ==="));
    Serial.println(F("Use: w=UP, s=DOWN, e=SELECT, q=BACK"));
    
    // Mostra memoria su LCD
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(F("System Ready"));
    lcd.setCursor(0, 1);
    lcd.print(F("Mem:"));
    lcd.print(getFreeMemory());
    lcd.print(F(" free"));
    
    delay(2000);
    needsRedraw = true;
}

void loop() {
    // Aggiorna controllers bottoni
    btn1.update();
    btn2.update(); 
    btn3.update();
    btn4.update();
    
    // Gestione input seriale
    MenuInput input = NONE;
    if (Serial.available()) {
        char c = Serial.read();
        if (c == 'w') input = UP;
        if (c == 's') input = DOWN;
        if (c == 'e') input = SELECT;
        if (c == 'q') input = BACK;
        while(Serial.available()) Serial.read();
        needsRedraw = true;
    }
    
    if (input != NONE && MenuState::currentPage) {
        MenuState::currentPage->handleInput(input);
    }
    
    // Refresh automatico sensori ogni 2s
    static unsigned long lastSensorUpdate = 0;
    if (millis() - lastSensorUpdate > 2000) {
        lastSensorUpdate = millis();
        if (MenuState::currentPage && 
            strcmp(MenuState::currentPage->getName(), "Sensor Status") == 0) {
            needsRedraw = true;
        }
    }
    
    // Render se necessario
    if (needsRedraw && MenuState::currentPage) {
        MenuState::currentPage->renderPage(lcd);
        needsRedraw = false;
    }
    
    // Monitor memoria ogni 15 secondi
    static unsigned long lastMemoryPrint = 0;
    if (millis() - lastMemoryPrint > 15000) {
        lastMemoryPrint = millis();
        Serial.print(F("[Runtime] Free memory: "));
        Serial.println(getFreeMemory());
        
        // // Mostra anche su LCD seconda riga
        // lcd.setCursor(0, 3);
        // lcd.print(F("Free:"));
        // lcd.print(getFreeMemory());
        // lcd.print(F("   "));
    }
}