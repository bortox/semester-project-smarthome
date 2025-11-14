#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
extern "C" {
  #include "i2cmaster.h"
}
#include "MyMenu.h"
#include "lights.h"
#include "sensors.h"

// --- SETUP HARDWARE ---
LiquidCrystal_I2C lcd(0x27, 20, 4);

// --- STATO GLOBALE DEL MENU ---
MenuItem* MenuState::currentPage = nullptr;
bool needsRedraw = true; // NUOVO: Flag per il ridisegno intelligente

// --- DISPOSITIVI REALI ---
SimpleLight l_bagno("Bagno", 5);
DimmableLight l_soggiorno("Soggiorno", 6);
RGBLight l_camera("Camera", 9, 10, 11);
LM75Sensor t_esterna("Temp Esterna");

// --- FUNZIONE DI COSTRUZIONE DEL MENU (Builder) ---
MenuItem* buildMenu() {
    MenuPage* root = new MenuPage("Menu Principale");

    MenuPage* lightsPage = new MenuPage("Luci", root);
    MenuPage* sensorsPage = new MenuPage("Sensori", root); // Corretto nome pagina
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
    soggiornoPage->addItem(new DimmerItem("Luminosita'", &l_soggiorno));

    MenuPage* coloreCameraPage = new MenuPage("Colore", cameraPage);
    cameraPage->addItem(new LightToggleItem(&l_camera));
    cameraPage->addItem(new RGBLightDimmerItem("Luminosita'", &l_camera));
    cameraPage->addItem(new SubMenuItem("Imposta Colore", coloreCameraPage));
    
    coloreCameraPage->addItem(new ColorSelectItem("Bianco Caldo", &l_camera, RGBColor::WarmWhite()));
    coloreCameraPage->addItem(new ColorSelectItem("Oceano", &l_camera, RGBColor::OceanBlue()));
    coloreCameraPage->addItem(new ColorSelectItem("Rosso", &l_camera, RGBColor::Red()));
    
    // === NUOVO: POPOLIAMO LA PAGINA SENSORI CORRETTAMENTE ===
    sensorsPage->addItem(new SensorDisplayItem<float, LM75Sensor>("Temp.", &t_esterna, "C"));
    // Aggiungi qui altri sensori se ne hai
    // E.g. sensorsPage->addItem(new SensorDisplayItem<int, LightSensor>("Luce", &mioSensoreLuce, "%"));

    return root;
}

void setup() {
    Serial.begin(9600);
    i2c_init();
    
    // --- Inizializzazione LCD più robusta ---
    lcd.init();
    delay(100); // Pausa per stabilizzare
    lcd.backlight();
    delay(100);

    t_esterna.begin();

    MenuState::currentPage = buildMenu();
    Serial.println("Sistema pronto.");
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
        needsRedraw = true; // Un input forza sempre il ridisegno
    }

    // --- LOGICA DI AGGIORNAMENTO DINAMICO (per sensori) ---
    static unsigned long lastSensorUpdate = 0;
    if (millis() - lastSensorUpdate > 1000) { // Controlla ogni secondo
        lastSensorUpdate = millis();
        // Se siamo nella pagina dei sensori, dobbiamo ridisegnare
        // per mostrare i valori aggiornati.
        if (MenuState::currentPage && MenuState::currentPage->getName() == "Sensori") {
             needsRedraw = true;
        }
    }

    // --- RENDERIZZAZIONE CONDIZIONALE (ANTI-FLICKERING) ---
    if (needsRedraw) {
        if (MenuState::currentPage) {
            MenuState::currentPage->renderPage(lcd);
        }
        needsRedraw = false; // Resetta il flag dopo aver disegnato
    }
}