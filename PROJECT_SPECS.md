# Home Automation Control System
**Platform:** Arduino Nano ATmega328P (2KB RAM, 30KB Flash)  
**Target:** Controllore domotico con menu LCD e input fisici

---

## 📋 Specifiche Funzionali

### Hardware Supportato
- **Display:** LCD I2C 20×4 caratteri (0x27)
- **Sensori:**
  - LM75 I2C temperatura (0x90)
  - Fotoresistore analogico (futuro)
  - PIR movimento digitale (futuro)
- **Attuatori:**
  - 2× LED semplici (D4, D7) - ON/OFF
  - 2× LED PWM (D5, D6) - Dimming 0-100%
  - 1× LED RGB PWM (futuro)
- **Input:**
  - 4× Bottoni (A0-A3) - Controllo diretto dispositivi
  - Serial console - Navigazione menu (w/s/e/q)
  - Rotary encoder (futuro) - Sostituzione serial

### Funzionalità Core
1. **Menu gerarchico dinamico**
   - Costruito runtime in base ai dispositivi registrati
   - Scrolling automatico (>3 item)
   - Navigazione con stack (BACK supportato)

2. **Controllo luci**
   - **Semplici:** Toggle diretto da lista o bottone fisico
   - **Dimmabili:** Sottomenu con Toggle + Slider brightness (±10%)
   - **Automazione (futuro):** Modalità Manual/Auto Light/Auto Light+Motion

3. **Monitoraggio sensori**
   - Display valore corrente in lista
   - Dettaglio con statistiche: Current/Min/Max/Average

4. **Event-driven architecture**
   - Bottone fisico → Device → EventSystem → Menu (aggiornamento automatico)
   - Nessun accoppiamento diretto Input ↔ UI

---

## 🏗️ Architettura Software

### Layer Structure

```
┌─────────────────────────────────────────┐
│  APPLICATION LAYER (main.cpp)          │
│  - Setup devices/buttons/menu          │
│  - Loop: update inputs/sensors/display │
└──────────────┬──────────────────────────┘
               │
┌──────────────▼──────────────────────────┐
│  UI LAYER (FlexibleMenu.h)             │
│  - MenuPage (container + event listener)│
│  - MenuItem (abstract item interface)  │
│  - NavigationManager (stack + render)  │
│  - MenuBuilder (factory dinamico)      │
└──────────────┬──────────────────────────┘
               │
┌──────────────▼──────────────────────────┐
│  DEVICE LAYER (Devices.h)              │
│  - SimpleLight / DimmableLight         │
│  - TemperatureSensor (con LM75)        │
│  - SensorStats (min/max/avg)           │
│  - DeviceFactory (allocazione devices) │
└──────────────┬──────────────────────────┘
               │
┌──────────────▼──────────────────────────┐
│  INPUT LAYER (PhysicalInput.h)         │
│  - ButtonInput (debouncing + linking)  │
│  - ButtonMode (ACTIVE_LOW/HIGH)        │
│  - InputManager (poll tutti bottoni)   │
└──────────────┬──────────────────────────┘
               │
┌──────────────▼──────────────────────────┐
│  CORE SYSTEM (CoreSystem.h)            │
│  - IDevice / IEventListener (interface)│
│  - EventSystem (observer pattern)      │
│  - DeviceRegistry (singleton)          │
│  - DynamicArray<T> (ottimizzato RAM)   │
└─────────────────────────────────────────┘
```

### Flusso Dati Tipico

**Esempio: Pressione bottone fisico**
```
1. ButtonInput::update() → rileva LOW (debounced)
2. ButtonInput::onButtonPressed()
3. → SimpleLight::toggle()
4. → EventSystem::emit(DeviceStateChanged)
5. → MenuPage::handleEvent() → forceRedraw()
6. → NavigationManager::update() → draw()
7. → LCD mostra nuovo stato
```

**Esempio: Cambio brightness da menu**
```
1. User: Serial 'w' → main.cpp traduce in 'U'
2. NavigationManager passa 'U' a MenuItem corrente
3. → BrightnessSliderItem::handleInput('U')
4. → DimmableLight::setBrightness(+10)
5. → EventSystem::emit(DeviceValueChanged)
6. → MenuPage::forceRedraw()
7. → LCD mostra barra progressiva aggiornata
```

---

## 💾 Ottimizzazioni RAM

### Tecniche Applicate
1. **DynamicArray con uint8_t**
   - `_size`, `_capacity` uint8_t invece di size_t
   - Risparmio: ~4B per array
   - Limite sicuro: 127 items max

2. **Enum con tipo esplicito**
   ```cpp
   enum class DeviceType : uint8_t { ... }
   enum EventType : uint8_t { ... }
   ```
   - Risparmio: 3B per enum

3. **SensorStats con int16_t**
   - Temperatura × 10 stored as int16_t
   - Min/Max/Avg calcolati da interi
   - Risparmio: ~10B per sensore

4. **Allocazione controllata**
   - DeviceFactory usa `new` ma verificato a compile-time
   - Nessun `delete` devices (lifetime = programma)
   - ButtonInput allocati con `new` (4× oggetti fissi)

5. **Stringhe in PROGMEM**
   - Usare `F()` macro ovunque possibile
   - Menu labels hardcoded (const char*)

### Budget RAM (2048B totali)
| Component | RAM Usage |
|-----------|-----------|
| Static data | ~771B |
| Heap (devices + menu) | ~596B |
| Stack | ~12B |
| **FREE** | **~669B** ✅ |

**Target minimo sicurezza:** 512B free  
**Status attuale:** ✅ OK con margine

---

## 🎨 Filosofia Design

### Principi Guida

#### 1. **KISS (Keep It Simple, Stupid)**
- Ogni classe ha **una sola responsabilità**
- Nessuna over-engineering
- Codice leggibile > codice "furbo"

**Esempio:**
```cpp
// ❌ BAD: Classe fa troppo
class Device {
    void update();
    void draw(LCD& lcd);
    void handleButton();
};

// ✅ GOOD: Separazione responsabilità
class IDevice { void update(); };
class MenuItem { void draw(LCD& lcd); };
class ButtonInput { void handleButton(); };
```

#### 2. **DRY (Don't Repeat Yourself)**
- Logica comune in classi base/template
- `DynamicArray<T>` usato ovunque serve crescita dinamica
- `MenuBuilder` factory methods evitano duplicazione

**Esempio:**
```cpp
// ❌ BAD: Codice duplicato per ogni lista
void buildLightsMenu() {
    for (auto device : devices) {
        if (device->isLight()) { ... }
    }
}
void buildSensorsMenu() {
    for (auto device : devices) {
        if (device->isSensor()) { ... }
    }
}

// ✅ GOOD: Template method
template<typename Predicate>
MenuPage* buildFilteredMenu(Predicate pred) {
    for (auto device : devices) {
        if (pred(device)) { ... }
    }
}
```

#### 3. **Decoupling (Low Coupling, High Cohesion)**
- `ButtonInput` NON conosce `MenuPage`
- Communication via `EventSystem`
- Device layer indipendente da UI layer

**Esempio:**
```cpp
// ❌ BAD: Accoppiamento forte
class ButtonInput {
    MenuPage* _menu;  // DIPENDENZA DIRETTA
    void onPress() { _menu->update(); }
};

// ✅ GOOD: Event-driven
class ButtonInput {
    void onPress() {
        EventSystem::emit(ButtonPressed);
        _linkedDevice->toggle();
    }
};
```

#### 4. **OOP (Object-Oriented Programming)**
- **Polimorfismo:** `IDevice`, `MenuItem` interfacce comuni
- **Ereditarietà:** `DimmableLight extends SimpleLight`
- **Incapsulamento:** Membri private, getter/setter pubblici

**Esempio:**
```cpp
// Polimorfismo in azione
DynamicArray<IDevice*> devices;
for (auto device : devices) {
    device->update();  // Chiamata polimorfica
}
```

#### 5. **Extensibility**
- Aggiungere nuovo device tipo? → Eredita `IDevice`
- Aggiungere menu item custom? → Eredita `MenuItem`
- Aggiungere sensore? → Eredita `Sensor<T>`

**Esempio futuro RGB Light:**
```cpp
class RGBLight : public IDevice {
    uint8_t _r, _g, _b;
    void setColor(uint8_t r, uint8_t g, uint8_t b);
    // DeviceRegistry lo gestisce automaticamente
};
```

---

## 📝 Stile Codice

### Naming Conventions
```cpp
// Classi: PascalCase
class NavigationManager { ... }

// Metodi pubblici: camelCase
void addItem(MenuItem* item);

// Membri privati: _camelCase (underscore prefix)
uint8_t _selected_index;

// Costanti: UPPER_SNAKE_CASE
const uint8_t DEBOUNCE_DELAY = 50;

// Enum: PascalCase per tipo, camelCase per valori
enum class DeviceType : uint8_t {
    LightSimple,
    LightDimmable
};

// Macro: UPPER_SNAKE_CASE
#define MAX_DEVICES 8
```

### Formatting
- **Indentazione:** 4 spazi (no tab)
- **Parentesi:** Stile K&R (opening brace same line)
  ```cpp
  if (condition) {
      // code
  }
  ```
- **Spazi:** Intorno operatori, dopo virgole
  ```cpp
  int result = a + b;
  func(arg1, arg2);
  ```
- **Line length:** Max 100 caratteri

### Comments
```cpp
// Singola linea per spiegazioni brevi
void toggle(); // Inverte stato LED

/* Multi-line per spiegazioni complesse
 * o documentazione funzioni
 */

// TODO: Implementare timeout automatico
// FIXME: Bug con più di 10 devices
// NOTE: Richiede pull-up esterno su A0-A3
```

### Memory Management
```cpp
// ✅ Preferisci stack allocation
SimpleLight light("LED", 4);

// ⚠️ Usa new solo se necessario dinamicità
MenuItem* item = new DeviceToggleItem(device);

// ❌ Evita delete (lifetime = programma per devices)
// Se proprio serve, usa RAII o smart pointer
```

### Type Safety
```cpp
// ✅ Usa typed enums
enum class ButtonMode : uint8_t { ACTIVE_LOW, ACTIVE_HIGH };

// ✅ Usa override keyword
void update() override;

// ✅ Usa const correttamente
const char* getName() const;
```

---

## 🔮 Roadmap Futuro

### Fase 2: Automazione Luci
```cpp
class AutomatedLight : public IEventListener {
    enum Mode { MANUAL, AUTO_LIGHT, AUTO_LIGHT_MOTION };
    Mode _mode;
    uint8_t _lightThreshold;
    
    void handleEvent(EventType type, IDevice* device, int value) {
        // Logica automatica basata su sensori
    }
};
```
- Menu item per selezione modalità
- Slider per threshold luce
- Event-driven: sensore luce/PIR → decision → attuazione

### Fase 3: Rotary Encoder
```cpp
class RotaryInput : public IEventListener {
    volatile int _position;
    void onClockwise() { /* emit UP event */ }
    void onCounterClockwise() { /* emit DOWN event */ }
    void onButtonPress() { /* emit SELECT event */ }
};
```
- Sostituisce serial input
- ISR per gestione interrupt
- Emette stessi comandi 'U'/'D'/'E' → zero refactoring menu

### Fase 4: RGB Light
```cpp
class RGBLight : public IDevice {
    uint8_t _r, _g, _b;
    // Sottomenu: Toggle / Set Color / Predefined Colors / Custom RGB
};
```
- Menu gerarchico per selezione colori
- Lista preset (Red/Green/Blue/White/...)
- Slider RGB individuali (0-255)

---

## 🐛 Troubleshooting

### "Low Memory Warning"
1. Ridurre `MAX_DEVICES` / `MAX_LISTENERS` in CoreSystem.h
2. Verificare nessun String object (usa const char*)
3. Ridurre profondità menu (max 3 livelli)

### "Bottoni non rispondono"
1. Verificare collegamenti: Bottone tra pin e GND
2. Check `ButtonMode`: ACTIVE_LOW per pull-up, ACTIVE_HIGH per pull-down
3. Testare con `digitalRead()` diretto in setup()

### "Menu non si aggiorna"
1. Verificare EventSystem::emit() chiamato dopo cambio stato
2. Check MenuPage registrato come listener
3. Verify forceRedraw() chiamato dopo input

### "Display corrotto"
1. I2C scan per verificare address (0x27)
2. Controllare alimentazione 5V stabile
3. Aggiungere delay(10) dopo lcd.clear()

---

## 📚 References

- **LiquidCrystal_I2C:** https://github.com/johnrickman/LiquidCrystal_I2C
- **LM75 Datasheet:** https://www.nxp.com/docs/en/data-sheet/LM75B.pdf
- **ATmega328P RAM:** https://www.microchip.com/en-us/product/ATmega328P

---

**Last Updated:** 2024  
**Author:** Andrea Bortolotti  
**License:** MIT
