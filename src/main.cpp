#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
extern "C" {
  #include "i2cmaster.h"
}
#include <Wire.h>
#include "MyMenu.h"      // Il tuo menu.h modificato per usare String
#include "lights.h"
#include "sensors.h"

// --- SETUP HARDWARE ---
//i2c_init()
LiquidCrystal_I2C lcd(0x27, 20, 4);

// --- DISPOSITIVI REALI ---
// Dichiariamo gli oggetti qui, così esistono per tutta la durata del programma.
// Le loro classi `Light` e `Sensor` usano ancora `const char*` per efficienza.
SimpleLight l_soggiorno("Bad", 5);
SimpleLight l_cucina("Køkken", 6);
SimpleLight l_giardino("Garden", 7);
LM75Sensor t_esterna("Sønderborg");

// --- STRUTTURA DEL MENU ---
// Usiamo puntatori per creare la struttura dinamicamente.
// ATTENZIONE: Questa memoria non viene mai liberata in questo esempio,
// perché il menu deve esistere per sempre. Non è un leak se è intenzionale.
MenuPage* rootPage;
MenuPage* subLights;
MenuPage* subSensors;

// Puntatore che tiene traccia della pagina attualmente attiva e visibile
MenuItem* currentActivePage;

MenuPage* buildMenu() {
    MenuPage* root = new MenuPage("Menu Principale");
    
    // --- PRIMO LIVELLO (già esistente) ---
    MenuPage* lightsPage = new MenuPage("Luci", root);
    MenuPage* sensorsPage = new MenuPage("Sensori", root);
    root->addItem(lightsPage);
    root->addItem(sensorsPage);
    
    // ... (codice per popolare Luci e Sensori come prima) ...

    // --- AGGIUNGIAMO IL SOTTOMENU IMPOSTAZIONI ---
    MenuPage* settingsPage = new MenuPage("Impostazioni", root);
    root->addItem(settingsPage);

    // --- SECONDO LIVELLO (dentro Impostazioni) ---
    MenuPage* displaySettingsPage = new MenuPage("Display", settingsPage);
    settingsPage->addItem(displaySettingsPage);
    MenuPage* networkSettingsPage = new MenuPage("Rete", settingsPage); // Esempio
    settingsPage->addItem(networkSettingsPage);

    // --- TERZO LIVELLO (dentro Display) ---
    // Per ora, usiamo un Item "finto" solo per dimostrazione
    // Lo sostituiremo con un vero ValueEditorItem
    class DummyItem : public MenuItem {
    public:
        DummyItem(const char* n) : MenuItem(n) {}
        void draw(LiquidCrystal_I2C& lcd, int row, bool isSelected) override {
            lcd.setCursor(1, row);
            lcd.print(getName());
        }
        MenuItem* select() override { return nullptr; }
    };
    
    displaySettingsPage->addItem(new DummyItem("Luminosita' LCD"));
    displaySettingsPage->addItem(new DummyItem("Contrasto"));
    displaySettingsPage->addItem(new DummyItem("Timeout Schermo"));
    
    return root;
}

void setup() {
    Serial.begin(9600);
    // Wire.begin(); // O 
    i2c_init(); // a seconda della libreria scelta
    
    // Init LCD
    lcd.init();
    lcd.backlight();

    // Init Dispositivi (se necessario)
    // t_esterna.begin(); // Questo va fatto SE la tua classe LM75Sensor ha un metodo begin()

    // --- COSTRUZIONE DEL MENU ---
    
    // 1. Crea le Pagine. 
    // Il costruttore di MenuPage si aspetta una String, ma il compilatore C++
    // convertirà automaticamente la stringa letterale "..." in un oggetto String.
    rootPage = new MenuPage("Main Menu", nullptr); 
    subLights = new MenuPage("Luci", rootPage);
    subSensors = new MenuPage("Sensori", rootPage);

    // 2. Popola la pagina LUCI.
    // Il costruttore di LightItem prende il `const char*` dalla luce e lo converte in String.
    subLights->addItem(new LightItem(&l_soggiorno));
    subLights->addItem(new LightItem(&l_cucina));
    subLights->addItem(new LightItem(&l_giardino));

    // 3. Popola la pagina SENSORI.
    // Anche qui, "Esterna" viene convertito in una String per il costruttore di SensorItem.
    subSensors->addItem(new SensorItem<LM75Sensor>("Esterna", &t_esterna, "C"));

    // 4. Collega le sottopagine al menu principale.
    rootPage->addItem(subLights);
    rootPage->addItem(subSensors);

    // 5. Imposta la pagina di partenza.
    currentActivePage = rootPage;
    // La prima visualizzazione
    static_cast<MenuPage*>(currentActivePage)->renderPage(lcd);
}

void loop() {
    // --- GESTIONE INPUT (Sostituire con la logica del Rotary Encoder) ---
    MenuInput input = NONE;
    
    if (Serial.available()) {
        char c = Serial.read();
        if (c == 'w') input = UP;
        if (c == 's') input = DOWN;
        if (c == 'e') input = SELECT;
        if (c == 'q') input = BACK;
        
        // Svuota il buffer seriale per evitare input multipli
        while(Serial.available()) Serial.read();
    }

    // --- LOGICA DI AGGIORNAMENTO DEL MENU ---
    if (input != NONE) {
        // La pagina corrente (che è sempre una MenuPage) gestisce l'input.
        // Il metodo `handleInput` può modificare `currentActivePage` passandogli il riferimento.
        static_cast<MenuPage*>(currentActivePage)->handleInput(input, currentActivePage); 

        // Ridisegna sempre la pagina corrente dopo un'azione dell'utente
        static_cast<MenuPage*>(currentActivePage)->renderPage(lcd);
    }
    
    // --- AGGIORNAMENTO AUTOMATICO DEI VALORI (OPZIONALE) ---
    // Se la pagina attiva è quella dei sensori, la ridisegniamo ogni secondo
    // per mostrare i valori aggiornati, anche senza input dell'utente.
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate > 1000) {
        if (currentActivePage == subSensors) {
            static_cast<MenuPage*>(currentActivePage)->renderPage(lcd);
        }
        lastUpdate = millis();
    }
}