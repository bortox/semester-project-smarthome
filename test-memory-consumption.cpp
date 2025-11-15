#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
extern "C" {
  #include "i2cmaster.h"
}
#include "MyMenu.h"
#include "lights.h"
#include "sensors.h"


// ===================================================================
// FUNZIONE PER IL MONITORAGGIO DELLA MEMORIA (versione per AVR)
// ===================================================================

// Variabili esterne fornite dal linker AVR-GCC. Non serve includere nulla.
extern char __heap_start;
extern char *__brkval; // Puntatore alla fine dell'heap

// Funzione che calcola la memoria libera tra la fine dell'heap e l'inizio dello stack.
// È la metrica più importante per la stabilità.
int getFreeMemory() {
  char top; // Variabile locale, il suo indirizzo è sul top dello stack
  return &top - (__brkval == 0 ? &__heap_start : __brkval);
}

// Stampa un report completo dello stato della RAM.
void printMemoryReport() {
  int free_mem = getFreeMemory();
  char *heap_end = __brkval;
  if (heap_end == 0) { // Se l'heap non è mai stato usato, punta all'inizio
      heap_end = &__heap_start;
  }
  
  int static_data_size = (int)&__heap_start - RAMSTART;
  int heap_size = heap_end - &__heap_start;
  int stack_size = RAMEND - (int)&free_mem + 1; // free_mem è l'indirizzo della var 'top'

  Serial.println(F(""));
  Serial.println(F("--- Memory Report ---"));
  Serial.print(F("Total RAM:        ")); Serial.print(RAMEND - RAMSTART + 1); Serial.println(F(" B"));
  Serial.println(F("---------------------"));
  Serial.print(F("Static Data:      ")); Serial.print(static_data_size); Serial.println(F(" B"));
  Serial.print(F("Heap Used:        ")); Serial.print(heap_size); Serial.println(F(" B"));
  Serial.print(F("Stack Used:       ")); Serial.print(stack_size); Serial.println(F(" B"));
  Serial.print(F("Free (Heap-Stack):  ")); Serial.print(free_mem); Serial.println(F(" B  <-- CRITICAL VALUE"));
  Serial.println(F("---------------------"));

  if (free_mem < 256) { // Una soglia di sicurezza ragionevole per un ATmega328
      Serial.println(F("!!! ATTENZIONE: Memoria libera molto bassa !!!"));
  }
}

// --- SETUP HARDWARE ---
LiquidCrystal_I2C lcd(0x27, 20, 4);

// --- STATO GLOBALE DEL MENU ---
MenuItem* MenuState::currentPage = nullptr;
bool needsRedraw = true;

// --- DISPOSITIVI REALI ---
SimpleLight l_bagno("Bagno", 5);
DimmableLight l_soggiorno("Soggiorno", 6);
RGBLight l_camera("Camera", 9, 10, 11);
LM75Sensor t_esterna("Temp Esterna");

// --- VARIABILI PER MONITORAGGIO ---
unsigned long lastMemoryPrintTime = 0;
const long memoryPrintInterval = 10000; // Stampa un report ogni 10 secondi

// --- FUNZIONE DI COSTRUZIONE DEL MENU (Builder) ---
MenuItem* buildMenu() {
    MenuPage* root = new MenuPage("Menu Principale");

    MenuPage* lightsPage = new MenuPage("Luci", root);
    MenuPage* sensorsPage = new MenuPage("Sensori", root);
    root->addItem(new SubMenuItem("Controllo Luci", lightsPage));
    root->addItem(new SubMenuItem("Stato Sensori", sensorsPage));

    MenuPage* bagnoPage = new MenuPage(l_bagno.getName(), lightsPage);
    MenuPage* soggiornoPage = new MenuPage(l_soggiorno.getName(), lightsPage);
    MenuPage* cameraPage = new MenuPage(l_camera.getName(), lightsPage);

    lightsPage->addItem(new SubMenuItem(l_bagno.getName(), bagnoPage));
    lightsPage->addItem(new SubMenuItem(l_soggiorno.getName(), soggiornoPage));
    lightsPage->addItem(new SubMenuItem(l_camera.getName(), cameraPage));

    bagnoPage->addItem(new LightToggleItem(&l_bagno));
    soggiornoPage->addItem(new LightToggleItem(&l_soggiorno));
    
    // NOTA: Qui stavi usando le vecchie classi, se hai fatto il refactoring
    // dovresti usare ValueEditorItem. Altrimenti lascia così.
    // Per ora mantengo le tue classi originali DimmerItem e RGBLightDimmerItem.
    soggiornoPage->addItem(new DimmerItem("Luminosita'", &l_soggiorno));

    MenuPage* coloreCameraPage = new MenuPage("Colore", cameraPage);
    cameraPage->addItem(new LightToggleItem(&l_camera));
    cameraPage->addItem(new RGBLightDimmerItem("Luminosita'", &l_camera));
    cameraPage->addItem(new SubMenuItem("Imposta Colore", coloreCameraPage));
    
    coloreCameraPage->addItem(new ColorSelectItem("Bianco Caldo", &l_camera, RGBColor::WarmWhite()));
    coloreCameraPage->addItem(new ColorSelectItem("Oceano", &l_camera, RGBColor::OceanBlue()));
    coloreCameraPage->addItem(new ColorSelectItem("Rosso", &l_camera, RGBColor::Red()));
    
    sensorsPage->addItem(new SensorDisplayItem<float, LM75Sensor>("Temp.", &t_esterna, "C"));

    return root;
}

void setup() {
    Serial.begin(9600);
    while(!Serial); // Attendi la connessione seriale
    Serial.println(F("\n\nSistema in avvio..."));

    i2c_init();
    
    lcd.init();
    delay(100); 
    lcd.backlight();
    delay(100);

    t_esterna.begin();

    // Stampa la memoria PRIMA di costruire il menu per vedere l'impatto
    Serial.println(F("--- Memoria prima della costruzione del menu ---"));
    printMemoryReport();

    MenuState::currentPage = buildMenu();

    // Stampa la memoria DOPO la costruzione del menu.
    // Questa è l'istantanea più importante!
    Serial.println(F("--- Memoria dopo la costruzione del menu ---"));
    printMemoryReport();
    
    Serial.println(F("Sistema pronto. In attesa di input..."));
}

void loop() {
    // --- GESTIONE INPUT ---
    MenuInput input = NONE;
    if (Serial.available()) {
        char c = Serial.read();
        if (c == 'w') input = UP;
        if (c == 's') input = DOWN;
        if (c == 'a') input = LEFT;
        if (c == 'd') input = RIGHT;
        if (c == 'e') input = SELECT;
        if (c == 'q') input = BACK;
        while(Serial.available()) Serial.read();
    }

    // --- LOGICA DI AGGIORNAMENTO STATO ---
    if (input != NONE) {
        if (MenuState::currentPage) {
            MenuState::currentPage->handleInput(input);
        }
        needsRedraw = true;
    }

    // --- LOGICA DI AGGIORNAMENTO DINAMICO (per sensori) ---
    static unsigned long lastSensorUpdate = 0;
    if (millis() - lastSensorUpdate > 1000) {
        lastSensorUpdate = millis();
        if (MenuState::currentPage && MenuState::currentPage->getName() == "Sensori") {
             needsRedraw = true;
        }
    }

    // --- RENDERIZZAZIONE CONDIZIONALE (ANTI-FLICKERING) ---
    if (needsRedraw) {
        if (MenuState::currentPage) {
            MenuState::currentPage->renderPage(lcd);
        }
        needsRedraw = false;
    }

    // --- MONITORAGGIO PERIODICO DELLA MEMORIA NEL LOOP ---
    unsigned long currentTime = millis();
    if (currentTime - lastMemoryPrintTime >= memoryPrintInterval) {
        lastMemoryPrintTime = currentTime;
        
        Serial.print(F("--- Report a runtime (T="));
        Serial.print(currentTime / 1000);
        Serial.println(F("s) ---"));
        printMemoryReport();
    }
}