#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include "MyMenu.h"      // La nostra classe custom
#include "lights.h"
#include "sensors.h"
extern "C" {
  #include <i2cmaster.h>
}

// --- SETUP HARDWARE ---
LiquidCrystal_I2C lcd(0x27, 20, 4);

// --- DISPOSITIVI (Le tue classi intoccabili) ---
SimpleLight l_esterno("Outside", 4);
SimpleLight l_soggiorno("Bad", 5);
SimpleLight l_cucina("Køkken", 6);
SimpleLight l_giardino("Garden", 7);
LM75Sensor t_esterna("Sønderborg"); // Assumiamo che LM75Sensor abbia getValue()
MovementSensor m_esterno("P1", 1);


// --- STRUTTURA DEL MENU ---
// Usiamo puntatori per mantenere tutto nello heap o globale
MenuPage* rootPage;
MenuPage* outside;
MenuPage* inside;
MenuPage* subLights;
MenuPage* subSensors;
MenuPage* outLights;

// Variabile per tenere traccia di dove siamo
MenuPage* currentPage;

void setup() {
    Serial.begin(9600);
    i2c_init();
    
    // Init LCD
    lcd.init();
    lcd.backlight();

    // Init Dispositivi
    t_esterna.begin();

    // --- COSTRUZIONE MENU (BUILDER) ---
    
    // 1. Crea le Pagine
    rootPage = new MenuPage("Main Menu", nullptr); // Root non ha genitore
    outside = new MenuPage("outside", rootPage);
    inside = new MenuPage("inside", rootPage);
    subLights = new MenuPage("Luci", inside);    // Figlio di Root
    outLights = new MenuPage("Luci", outside);
    subSensors = new MenuPage("Sensori", outside); // Figlio di Root

    // 2. Popola pagina LUCI con i wrapper delle luci
    subLights->addItem(new LightItem(&l_soggiorno));
    subLights->addItem(new LightItem(&l_cucina));
    subLights->addItem(new LightItem(&l_giardino));
    outLights->addItem(new LightItem(&l_esterno));
    // 3. Popola pagina SENSORI
    // Qui sfruttiamo il template SensorItem
    subSensors->addItem(new SensorItem<LM75Sensor>("Esterna", &t_esterna, "C"));
    subSensors->addItem(new SensorItem<MovementSensor>("Diocane", &m_esterno, ""));

    // 4. Collega le sottopagine alla Root
    rootPage->addItem(subLights);
    rootPage->addItem(subSensors);

    // Start
    currentPage = rootPage;
    currentPage->renderPage(lcd);
}

void loop() {
    // --- SIMULAZIONE INPUT (Sostituisci con il tuo codice Rotary Encoder) ---
    // Qui dovresti leggere il tuo encoder e mappare in MenuInput::UP, DOWN, etc.
    
    MenuInput input = NONE;
    
    if (Serial.available()) {
        char c = Serial.read();
        if (c == 'w') input = UP;
        if (c == 's') input = DOWN;
        if (c == 'e') input = SELECT;
        if (c == 'q') input = BACK;
        
        // Pulisci input buffer seriale per test puliti
        while(Serial.available()) Serial.read();
    }

    // --- AGGIORNAMENTO MENU ---
    // Se c'è stato un input, lo passiamo alla pagina corrente
    if (input != NONE) {
        // Passiamo un riferimento a currentPage, così handleInput può cambiarla!
        MenuItem* active = currentPage; 
        currentPage->handleInput(input, active); 
        
        // Se la pagina è cambiata, aggiorniamo il puntatore globale
        if (active != currentPage) {
            currentPage = (MenuPage*)active; // Cast sicuro perché navighiamo solo tra pagine
        }

        // Ridisegna solo se c'è stato input
        currentPage->renderPage(lcd);
    }
    
    // Opzionale: Ridisegna sensori ogni secondo anche senza input
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate > 1000 && currentPage == subSensors) {
        currentPage->renderPage(lcd);
        lastUpdate = millis();
    }
}