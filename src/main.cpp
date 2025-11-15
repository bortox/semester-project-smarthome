#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include "config.h" // UNICO INCLUDE per tutta la configurazione!

// ===================================================================
// CONFIGURAZIONE APPLICAZIONE
// ===================================================================
LiquidCrystal_I2C lcd(0x27, 16, 2);

// STATO GLOBALE DELL'APPLICAZIONE
MenuItem* MenuState::currentPage = nullptr;
bool needsRedraw = true;


void setup() {
    // Inizializzazione delle comunicazioni
    Serial.begin(9600);
    i2c_init();

    // Inizializzazione dei dispositivi hardware
    lcd.init();
    lcd.backlight();
    tempSensor.begin(); // L'oggetto è definito in HomeConfig.h

    // Costruisci il menu e imposta la pagina iniziale
    // La funzione buildMenu() e tutti gli oggetti (luci, etc.) provengono da HomeConfig.h
    MenuState::currentPage = buildMenu();
    
    Serial.println(F("Sistema pronto. Usa 'w,s,e,q' per navigare."));
}

void loop() {
    // --- 1. GESTISCI I CONTROLLI FISICI DIRETTI ---
    // Questi vengono eseguiti ad ogni ciclo per la massima reattività.
    rgbController.update();
    dimmerController.update();

    // --- 2. GESTISCI L'INPUT DEL MENU DALLA SERIALE ---
    MenuInput input = NONE;
    if (Serial.available()) {
        char c = Serial.read();
        if (c == 'w') input = UP;
        if (c == 's') input = DOWN;
        if (c == 'e') input = SELECT; // 'e' per "Enter"
        if (c == 'q') input = BACK;   // 'q' per "Quit/Back"
        while(Serial.available()) Serial.read();
    }
    
    // --- 3. AGGIORNA LO STATO DEL MENU ---
    if (input != NONE) {
        if (MenuState::currentPage) {
            MenuState::currentPage->handleInput(input);
        }
        needsRedraw = true;
    }

    // --- 4. GESTISCI AGGIORNAMENTI AUTOMATICI DEL DISPLAY (SENSORI) ---
    static unsigned long lastSensorUpdate = 0;
    if (millis() - lastSensorUpdate > 1000) {
        lastSensorUpdate = millis();
        if (MenuState::currentPage && MenuState::currentPage->getName() == "Sensor Statusw") {
             needsRedraw = true;
        }
    }

    // --- 5. RENDERIZZA IL MENU (SOLO SE NECESSARIO) ---
    if (needsRedraw) {
        if (MenuState::currentPage) {
            MenuState::currentPage->renderPage(lcd);
        }
        needsRedraw = false;
    }
}