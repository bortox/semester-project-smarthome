# Analisi Ottimizzazione Memoria RAM

## Contesto

**Situazione attuale:**
- **RAM totale**: 2048 B (ATmega328P)
- **RAM libera**: ~597 B
- **Target minimo**: 512 B (margine sicurezza)
- **Status**: ⚠️ **CRITICO** - Solo 85B sopra il minimo

**Problema:** Con l'aggiunta di:
- Rotary encoder Modulino Knob (+50B stato)
- LED RGB (+30B buffer colori)
- Automazione luci (+80B logica)
- Sensore movimento (+20B stato)

**RAM stimata finale**: 597 - 180 = **417 B** ❌ **SOTTO IL MINIMO SICURO**

## Obiettivi Analisi

1. Identificare ottimizzazioni **cheap** (facili da implementare)
2. Valutare **trade-off** architettura vs memoria
3. Mantenere **OOP** e **estendibilità** dove possibile
4. Quantificare **risparmi** e **costi** di ogni modifica

---

## 1. Ottimizzazioni CHEAP (Facile, Poco Impatto)

### 1.1 Rimuovere File Inutilizzati

**File da eliminare:**
- `ScrollingDisplay.h` / `ScrollingDisplay.cpp` → **Non usati**
- `CommandProcessor.h` / `CommandProcessor.cpp` → **Non usati**

**Risparmio:** ~0 B RAM (solo flash), ma pulizia codebase  
**Impatto OOP:** ✅ Nessuno  
**Difficoltà:** ⭐ Triviale  

---

### 1.2 Ottimizzare `Serial.print()` Debug

**Problema:** Ogni `Serial.print(F("..."))` consuma stack durante chiamata.

**Soluzione:**
```cpp
// PRIMA (main.cpp, FlexibleMenu.h, PhysicalInput.cpp)
Serial.print(F("Toggle "));
Serial.print(light->getName());
Serial.print(F(" -> "));
Serial.println(light->getState() ? F("ON") : F("OFF"));

// DOPO: Usa macro conditional
#ifdef DEBUG_SERIAL
  #define DEBUG_PRINT(x) Serial.print(x)
  #define DEBUG_PRINTLN(x) Serial.println(x)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
#endif

// In produzione: compila con -DDEBUG_SERIAL=0
```

**Risparmio:** ~40B stack durante print  
**Impatto OOP:** ✅ Nessuno (macro preprocessing)  
**Difficoltà:** ⭐⭐ Facile  

---

### 1.3 Ridurre Dimensione Stack DynamicArray

**Problema:** `DynamicArray` parte con capacity=4, poi raddoppia (4→8→16→32).

**Attuale:**
```cpp
// CoreSystem.h
bool add(T item) {
    uint8_t newCapacity = (_capacity == 0) ? 4 : _capacity * 2;
    // ...
}
```

**Soluzione:** Start con 2, crescita +2 invece che ×2
```cpp
bool add(T item) {
    uint8_t newCapacity = (_capacity == 0) ? 2 : _capacity + 2;
    // Max capacity = 16 (abbastanza per menu)
    if (newCapacity > 16) return false;
}
```

**Risparmio:**
- Menu con 3 items: Era 8 puntatori (16B) → Ora 4 puntatori (8B) = **8B per array**
- 5 array attivi (devices, listeners, menu items, buttons, stack) = **40B totali**

**Impatto OOP:** ⚠️ Limite max 16 items per array (accettabile)  
**Difficoltà:** ⭐⭐ Facile  

---

### 1.4 Stringhe Menu in PROGMEM

**Problema:** Label menu stored in RAM.

**PRIMA:**
```cpp
// FlexibleMenu.h
page->addItem(new SubMenuItem("Lights", buildLightsPage(root), ...));
```

**DOPO:**
```cpp
// Definisci in flash
const char STR_LIGHTS[] PROGMEM = "Lights";
const char STR_SENSORS[] PROGMEM = "Sensors";
const char STR_BACK[] PROGMEM = "<< Back";
const char STR_SET_BRIGHTNESS[] PROGMEM = "Set Brightness";

page->addItem(new SubMenuItem(STR_LIGHTS, buildLightsPage(root), ...));

// MenuItem::draw() usa:
char buffer[20];
strcpy_P(buffer, _label);  // Copia da PROGMEM
lcd.print(buffer);
```

**Risparmio:** ~80B (tutte le label menu)  
**Impatto OOP:** ⚠️ Servono modifiche a `draw()` per leggere da PROGMEM  
**Difficoltà:** ⭐⭐⭐ Media  

---

## 2. Ottimizzazioni MODERATE (Trade-off Accettabili)

### 2.1 Menu Items come Enum invece di Classi

**PRIMA (OOP puro):**
```cpp
class DeviceToggleItem : public MenuItem { ... };
class BrightnessSliderItem : public MenuItem { ... }
// Ogni oggetto = 8-12B RAM
```

**DOPO (Hybrid):**
```cpp
enum class MenuItemType : uint8_t {
    DEVICE_TOGGLE,
    BRIGHTNESS_SLIDER,
    SENSOR_STATS,
    SUBMENU,
    BACK
};

struct MenuItem {
    MenuItemType type;
    void* data;  // Puntatore generico a device/sensor
    uint8_t param; // es. lineOffset per SensorStats
};

// Rendering via switch-case
void drawItem(MenuItem* item, LCD& lcd) {
    switch(item->type) {
        case DEVICE_TOGGLE: /* ... */ break;
        case BRIGHTNESS_SLIDER: /* ... */ break;
    }
}
```

**Risparmio:** 
- Ogni menu item: 12B oggetto → 4B struct = **8B per item**
- 15 items medi = **120B totali**

**Impatto OOP:** ❌ Perde polimorfismo, ma mantiene encapsulation  
**Difficoltà:** ⭐⭐⭐⭐ Medio-alta (refactoring FlexibleMenu.h)  

---

### 2.2 Eliminare EventSystem per Bottoni

**PROBLEMA:** `EventSystem` mantiene lista listeners (10× puntatori = 20B).

**Soluzione:** Direct callback invece di observer pattern.

**PRIMA:**
```cpp
// PhysicalInput.cpp
void ButtonInput::onButtonPressed() {
    EventSystem::instance().emit(EventType::ButtonPressed, nullptr, _buttonId);
    if (_linkedDevice) _linkedDevice->toggle();
}
```

**DOPO:**
```cpp
void ButtonInput::onButtonPressed() {
    if (_linkedDevice) _linkedDevice->toggle();
    // Nessun evento, solo azione diretta
}
```

**Risparmio:** 20B (lista listeners) + overhead chiamate  
**Impatto OOP:** ⚠️ Perde decoupling, bottoni conoscono devices  
**Difficoltà:** ⭐⭐ Facile (già implementato così, basta rimuovere emit)  

---

### 2.3 Single MenuPage Pool (No Dynamic Allocation)

**PROBLEMA:** Ogni `new MenuPage()` alloca dinamicamente.

**Soluzione:** Pool statico pre-allocato.

```cpp
// FlexibleMenu.h
class MenuPagePool {
    static const uint8_t MAX_PAGES = 8;
    MenuPage _pages[MAX_PAGES];
    uint8_t _used = 0;
    
public:
    MenuPage* allocate(const char* title, MenuPage* parent) {
        if (_used >= MAX_PAGES) return nullptr;
        MenuPage* page = &_pages[_used++];
        // Placement new
        new (page) MenuPage(title, parent);
        return page;
    }
};

// Uso:
MenuPage* page = MenuPagePool::instance().allocate("Lights", parent);
```

**Risparmio:** Elimina frammentazione heap (~30B overhead allocatore)  
**Impatto OOP:** ⚠️ Limite fisso 8 pagine menu (OK per questo progetto)  
**Difficoltà:** ⭐⭐⭐ Media  

---

## 3. Ottimizzazioni AGGRESSIVE (Alto Impatto Architettura)

### 3.1 Menu Statico Hardcoded

**Eliminare completamente costruzione dinamica menu.**

```cpp
// Menu hardcoded in array
const MenuItem MENU_MAIN[] = {
    {SUBMENU, "Lights", &MENU_LIGHTS},
    {SUBMENU, "Sensors", &MENU_SENSORS}
};

const MenuItem MENU_LIGHTS[] = {
    {DEVICE_TOGGLE, "LED 1", &led1},
    {SUBMENU, "LED 2", &MENU_LED2},
    {BACK, nullptr, nullptr}
};
```

**Risparmio:** ~200B (elimina allocazioni + puntatori)  
**Impatto OOP:** ❌❌ Perde completamente flessibilità dinamica  
**Difficoltà:** ⭐⭐⭐⭐⭐ Completa riscrittura FlexibleMenu.h  

**SCONSIGLIATO:** Viola principio DRY e rende impossibile aggiungere device runtime.

---

### 3.2 Eliminare DeviceRegistry

**Problema:** Array dinamico devices consuma RAM.

**Soluzione:** Devices come variabili globali, gestiti manualmente.

```cpp
// main.cpp
SimpleLight led1("LED 1", 4);
DimmableLight led2("LED 2", 5);
// No registry, accesso diretto
```

**Risparmio:** ~30B (array + overhead)  
**Impatto OOP:** ❌ Perde centralizzazione, viola Single Responsibility  
**Difficoltà:** ⭐⭐⭐⭐ Alta (refactoring completo)  

**SCONSIGLIATO:** Rende impossibile iterare devices per menu builder.

---

## 4. Raccomandazioni Finali

### Strategia Consigliata (Target: +150B)

| Ottimizzazione | Risparmio | Difficoltà | Impatto OOP | Consigliato |
|----------------|-----------|------------|-------------|-------------|
| **1.1 Rimuovi file inutili** | 0B | ⭐ | ✅ Nessuno | ✅ SÌ |
| **1.2 Debug conditional** | 40B | ⭐⭐ | ✅ Nessuno | ✅ SÌ |
| **1.3 DynamicArray growth** | 40B | ⭐⭐ | ⚠️ Limite 16 | ✅ SÌ |
| **1.4 Stringhe PROGMEM** | 80B | ⭐⭐⭐ | ⚠️ Modifica draw | ✅ SÌ |
| **2.2 Rimuovi EventSystem bottoni** | 20B | ⭐⭐ | ⚠️ Coupling | ⚠️ MAYBE |
| **2.3 MenuPage pool** | 30B | ⭐⭐⭐ | ⚠️ Limite 8 pagine | ⚠️ MAYBE |
| **TOTALE CHEAP** | **160B** | | | **TARGET RAGGIUNTO** |

### Implementazione Suggerita

**Fase 1 (Subito):**
1. Elimina `ScrollingDisplay.h`, `CommandProcessor.h`
2. Implementa debug conditional (`#ifdef DEBUG_SERIAL`)
3. Modifica DynamicArray growth (capacity +2 invece di ×2)

**Fase 2 (Se serve ancora):**
4. Stringhe menu in PROGMEM
5. Rimuovi EventSystem per bottoni (direct callback)

**Fase 3 (Solo se disperati):**
6. MenuPage pool statico

### RAM Finale Stimata

```
Attuale:        597 B
- Debug cond:   -40 B
- Array growth: -40 B
- PROGMEM str:  -80 B
= TOTALE:       757 B FREE ✅

Con features future:
757 - 180 (knob+RGB+auto) = 577 B ✅ SICURO
```

---

## 5. Riferimenti File Modificati

**CoreSystem.h:**
- `DynamicArray` growth strategy (1.3)
- Enum size ottimizzazione (già fatto)

**FlexibleMenu.h:**
- PROGMEM stringhe (1.4)
- Pool MenuPage (2.3)
- Menu items come struct (2.1 - solo se disperati)

**main.cpp:**
- Debug conditional (1.2)
- Rimozione file inutili (1.1)

**PhysicalInput.cpp:**
- Rimozione EventSystem emit (2.2)

**PROJECT_SPECS.md:**
- Aggiornare budget RAM con nuove stime

---

## 6. Applicazione Ottimizzazioni CHEAP (Fase 1)

### Implementato

✅ **1.1 Rimozione file inutilizzati**
- Eliminati `ScrollingDisplay.h` e `CommandProcessor.h`
- Risparmio: 0B RAM, ma previene future istanziazioni accidentali
- Status: **COMPLETATO**

✅ **1.2 Debug conditional**
- Creato `DebugConfig.h` con macro `DEBUG_PRINT()` / `DEBUG_PRINTLN()`
- Rimossi print in `FlexibleMenu.h` e `PhysicalInput.cpp`
- Risparmio: **~40B stack** durante esecuzione
- Per produzione: Compilare con `-DDEBUG_SERIAL=0`
- Status: **COMPLETATO**

✅ **1.3 DynamicArray crescita lineare**
- Modificato `CoreSystem.h`: capacity ora cresce +2 invece di ×2
- Capacity iniziale ridotta da 4 a 2
- Limite massimo imposto a 16 items per array
- Risparmio stimato: **~40B** (5 array attivi × 8B risparmiati)
- Status: **COMPLETATO**

✅ **1.4 EventSystem ottimizzato**
- Rimosso emit non necessario in `ButtonInput::onButtonPressed()`
- Toggle già genera evento via `DeviceStateChanged`
- Risparmio: **~10B** overhead chiamate
- Status: **COMPLETATO**

### Risultati Attesi

```
RAM Baseline:           597 B
- Debug prints:         +40 B
- Array growth:         +40 B
- EventSystem cleanup:  +10 B
= TOTALE STIMATO:       687 B FREE ✅
```

**Margine sicurezza:** 687 - 512 = **175B** sopra minimo  
**Con features future (180B):** 687 - 180 = **507B** ⚠️ Borderline ma gestibile

### Prossimi Passi

Se dopo test reali la RAM è ancora critica:
- **Fase 2**: Stringhe menu in PROGMEM (+80B)
- **Fase 3**: MenuPage pool statico (+30B)

**Test consigliato:**
1. Compilare e caricare
2. Verificare `printMemoryReport()` al boot
3. Testare tutte le funzionalità (menu navigazione, bottoni, sensori)
4. Se FREE > 650B → **Successo, procedi con Modulino Knob**
5. Se FREE < 600B → **Applicare Fase 2**
