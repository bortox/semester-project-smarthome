#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include "MyMenu.h" // Assicurati di avere le tue classi del menu
#include "lights.h" // E le tue classi delle luci

// ===================================================================
// CONFIGURAZIONE HARDWARE
// ===================================================================
LiquidCrystal_I2C lcd(0x27, 20, 4);

// Pin dei pulsanti di navigazione
const uint8_t BUTTON_UP_PIN     = A0;
const uint8_t BUTTON_DOWN_PIN   = A1;
const uint8_t BUTTON_SELECT_PIN = A2;
const uint8_t BUTTON_BACK_PIN   = A3;

// Pin dei LED (che rappresentano le nostre luci)
const uint8_t LED_PIN_1 = 4;
const uint8_t LED_PIN_2 = 5;
const uint8_t LED_PIN_3 = 6;
const uint8_t LED_PIN_4 = 7;

// --- OGGETTI LUCE ---
// Creiamo oggetti reali per le luci che corrispondono ai LED
SimpleLight luce1("Luce 1", LED_PIN_1);
SimpleLight luce2("Luce 2", LED_PIN_2);
SimpleLight luce3("Luce 3", LED_PIN_3);
SimpleLight luce4("Luce 4", LED_PIN_4);

// --- STATO DEL MENU ---
MenuItem* MenuState::currentPage = nullptr;
bool needsRedraw = true;

// ===================================================================
// FUNZIONE PER LEGGERE L'INPUT DAI PULSANTI (con debounce)
// ===================================================================
MenuInput readInput() {
    static unsigned long lastPressTime = 0;
    const long debounceDelay = 200; // Aumentato per evitare pressioni multiple

    if (millis() - lastPressTime < debounceDelay) {
        return NONE; // Troppo presto dalla scorsa pressione, ignora
    }

    if (digitalRead(BUTTON_UP_PIN) == LOW) {
        lastPressTime = millis();
        return UP;
    }
    if (digitalRead(BUTTON_DOWN_PIN) == LOW) {
        lastPressTime = millis();
        return DOWN;
    }
    if (digitalRead(BUTTON_SELECT_PIN) == LOW) {
        lastPressTime = millis();
        return SELECT;
    }
    if (digitalRead(BUTTON_BACK_PIN) == LOW) {
        lastPressTime = millis();
        return BACK;
    }

    return NONE;
}

// ===================================================================
// COSTRUZIONE DELLA STRUTTURA DEL MENU (ARCHITETTURA CORRETTA)
// ===================================================================
MenuItem* buildMenu() {
    // 1. Livello radice
    MenuPage* root = new MenuPage("Menu Principale");

    // 2. Pagine di secondo livello
    MenuPage* lightsPage = new MenuPage("Controllo Luci", root);
    
    // Aggiungiamo i link dalla radice alle pagine di secondo livello
    root->addItem(new SubMenuItem("Luci", lightsPage));
    // Qui potresti aggiungere un SubMenuItem "Sensori" che punta a una "sensorsPage"

    // 3. Pagine specifiche per ogni luce (QUESTO RISOLVE IL BLOCCO)
    MenuPage* pageLuce1 = new MenuPage(luce1.getName(), lightsPage);
    MenuPage* pageLuce2 = new MenuPage(luce2.getName(), lightsPage);
    MenuPage* pageLuce3 = new MenuPage(luce3.getName(), lightsPage);
    MenuPage* pageLuce4 = new MenuPage(luce4.getName(), lightsPage);

    // Aggiungiamo i link dalla pagina "Luci" alle pagine specifiche
    lightsPage->addItem(new SubMenuItem(luce1.getName(), pageLuce1));
    lightsPage->addItem(new SubMenuItem(luce2.getName(), pageLuce2));
    lightsPage->addItem(new SubMenuItem(luce3.getName(), pageLuce3));
    lightsPage->addItem(new SubMenuItem(luce4.getName(), pageLuce4));

    // 4. Aggiungiamo gli item di controllo DENTRO le pagine specifiche
    pageLuce1->addItem(new LightToggleItem(&luce1));
    pageLuce2->addItem(new LightToggleItem(&luce2));
    pageLuce3->addItem(new LightToggleItem(&luce3));
    pageLuce4->addItem(new LightToggleItem(&luce4));
    // Se una luce fosse dimmerabile, qui aggiungeresti:
    // pageLuceX->addItem(new DimmerItem("Luminosita", &luceDimmerabileX));

    return root;
}


void setup() {
    Serial.begin(9600);
    
    // Inizializza LCD
    lcd.init();
    lcd.backlight();

    // Inizializza i pulsanti con resistenza di pull-up interna
    pinMode(BUTTON_UP_PIN, INPUT_PULLUP);
    pinMode(BUTTON_DOWN_PIN, INPUT_PULLUP);
    pinMode(BUTTON_SELECT_PIN, INPUT_PULLUP);
    pinMode(BUTTON_BACK_PIN, INPUT_PULLUP);

    // I pin dei LED sono già inizializzati nel costruttore di SimpleLight

    // Costruisci il menu e imposta la pagina iniziale
    MenuState::currentPage = buildMenu();
    
    Serial.println("Sistema pronto. Usa i pulsanti per navigare.");
}

void loop() {
    // 1. Leggi l'input
    MenuInput input = readInput();

    // 2. Aggiorna lo stato del menu in base all'input
    if (input != NONE) {
        if (MenuState::currentPage) {
            MenuState::currentPage->handleInput(input);
        }
        needsRedraw = true; // Ogni input richiede un ridisegno
    }

    // 3. Disegna l'interfaccia (solo se necessario)
    if (needsRedraw) {
        if (MenuState::currentPage) {
            MenuState::currentPage->renderPage(lcd);
        }
        needsRedraw = false; // Resetta il flag
    }
}