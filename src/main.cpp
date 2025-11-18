#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include "config.h"       // Unico include per tutta la configurazione!
#include "MemoryMonitor.h"  // Includiamo il nostro nuovo monitor

// ===================================================================
// CONFIGURAZIONE APPLICAZIONE
// ===================================================================
LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLS, LCD_ROWS); // Parametri da config.h

// STATO GLOBALE DELL'APPLICAZIONE
MenuItem* MenuState::currentPage = nullptr;
bool needsRedraw = true;


void setup() {
    // 1. Inizializzazione delle comunicazioni
    Serial.begin(9600);
    while(!Serial); // Attendi la connessione per vedere i messaggi di avvio
    Serial.println(F("\n\n=== Smart Home System Initializing ==="));
    i2c_init();

    // 2. Report memoria PRIMA dell'allocazione dinamica
    Serial.println(F("\n[Memory Check 1/3] Before menu construction:"));
    printMemoryReport();

    // 3. Inizializzazione dei dispositivi hardware
    lcd.init();
    lcd.backlight();
    tempSensor.begin(); // L'oggetto è definito in config.h

    // 4. Costruisci il menu (questa è l'operazione che alloca più memoria)
    MenuState::currentPage = buildMenu();
    
    // 5. Report memoria DOPO l'allocazione del menu
    Serial.println(F("\n[Memory Check 2/3] After menu construction (Heap is now set):"));
    printMemoryReport();
    
    Serial.println(F("\nSystem Ready. Use 'w,a,s,d,e,q' for navigation."));
}

void loop() {
    // --- 1. GESTISCI I CONTROLLI FISICI DIRETTI (se presenti) ---
    // E.g., lettura di potenziometri o interruttori che non fanno parte del menu.
    // rgbController.update();
    // dimmerController.update();

    // --- 2. GESTISCI L'INPUT DEL MENU CENTRALE ---
    MenuInput input = NONE;
    if (Serial.available()) {
        char c = Serial.read();
        if (c == 'w') input = UP;
        if (c == 's') input = DOWN;
        if (c == 'e') input = SELECT;
        if (c == 'q') input = BACK;
        while(Serial.available()) Serial.read();
    }
    
    // --- 3. AGGIORNA LO STATO DEL MENU IN BASE ALL'INPUT ---
    if (input != NONE) {
        if (MenuState::currentPage) {
            MenuState::currentPage->handleInput(input);
        }
        needsRedraw = true; // Forza il ridisegno dopo un'azione
    }

    // --- 4. GESTISCI AGGIORNAMENTI AUTOMATICI DEL DISPLAY (es. SENSORI) ---
    static unsigned long lastSensorUpdate = 0;
    if (millis() - lastSensorUpdate > 2000) { // Aggiorna ogni 2 secondi
        lastSensorUpdate = millis();
        // Controlla se il nome della pagina corrente è quello dei sensori
        if (MenuState::currentPage && MenuState::currentPage->getName() == "Sensor Status") {
             needsRedraw = true;
        }
    }

    // --- 5. RENDERIZZA IL MENU (SOLO SE NECESSARIO) ---
    if (needsRedraw) {
        if (MenuState::currentPage) {
            MenuState::currentPage->renderPage(lcd);
        }
        needsRedraw = false; // Resetta il flag
    }
    
    // --- 6. MONITORAGGIO PERIODICO A RUNTIME ---
    static unsigned long lastMemoryPrint = 0;
    if (millis() - lastMemoryPrint > 10000) { // Stampa ogni 10 secondi
        lastMemoryPrint = millis();
        Serial.print(F("\n[Memory Check 3/3] Runtime at T="));
        Serial.print(millis() / 1000);
        Serial.println(F("s:"));
        printMemoryReport();
    }
}