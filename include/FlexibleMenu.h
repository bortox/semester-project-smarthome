#ifndef FLEXIBLE_MENU_H
#define FLEXIBLE_MENU_H

#include <Arduino.h>
#include <avr/pgmspace.h>
extern "C" {
    #include "lcd.h"
}
#include "Devices.h"

class MenuPage;
class NavigationManager;
class SubMenuItem;

enum class MenuItemType {
    GENERIC,
    SUBMENU
};

class MenuItem {
public:
    virtual ~MenuItem() {}
    virtual void draw(uint8_t row, bool selected) = 0;
    virtual bool handleInput(char key) { return false; } // FIX: Ora ritorna bool
    virtual MenuItemType getType() const { return MenuItemType::GENERIC; }
    virtual bool relatesTo(IDevice* device) { return false; }

protected:
    // Helper per stampare stringhe da Flash o RAM
    void printLabel(const char* str) {
        LCD_write_str(str);
    }
    
    void printLabel(const __FlashStringHelper* f_str) {
        char buf[21];
        strncpy_P(buf, (const char*)f_str, 20);
        buf[20] = 0;
        LCD_write_str(buf);
    }
};

class MenuPage : public MenuItem, public IEventListener {
private:
    const __FlashStringHelper* _title;
    DynamicArray<MenuItem*> _items;
    MenuPage* _parent;
    size_t _selected_index;
    size_t _scroll_offset;
    bool _needs_redraw;
    
    friend class NavigationManager;

public:
    MenuPage(const __FlashStringHelper* title, MenuPage* parent = nullptr) 
        : _title(title), _parent(parent), _selected_index(0), _scroll_offset(0), 
          _needs_redraw(true) {
        EventSystem::instance().addListener(this, EventType::DeviceStateChanged);
        EventSystem::instance().addListener(this, EventType::DeviceValueChanged);
        EventSystem::instance().addListener(this, EventType::SensorUpdated);
    }

    virtual ~MenuPage() {
        EventSystem::instance().removeListener(this);
        for (size_t i = 0; i < _items.size(); i++) delete _items[i];
    }

    void addItem(MenuItem* item) { _items.add(item); }
    MenuItem* getItem(size_t idx) { return _items[idx]; }
    size_t getItemsCount() const { return _items.size(); }
    MenuPage* getParent() const { return _parent; }
    size_t getSelectedIndex() const { return _selected_index; }

    void draw(uint8_t row, bool selected) override {
        LCD_set_cursor(0, row);
        LCD_write_str(selected ? "> " : "  ");
        printLabel(_title);
    }

    // FIX: DICHIARAZIONE con bool come return type
    bool handleInput(char key) override;
    void handleEvent(EventType type, IDevice* device, int value) override;
    
    bool needsRedraw() const { return _needs_redraw; }
    void clearRedraw() { _needs_redraw = false; }
    void forceRedraw() { _needs_redraw = true; }
};

class NavigationManager {
private:
    DynamicArray<MenuPage*> _stack;
    bool _initialized;

    NavigationManager() : _initialized(false) {}

public:
    static NavigationManager& instance() {
        static NavigationManager inst;
        return inst;
    }

    void setLCD() { _initialized = true; }

    void initialize(MenuPage* root) {
        _stack.add(root);
        draw();
    }

    // JIT: Aggiunge una pagina creata dinamicamente allo stack
    void pushPage(MenuPage* page) {
        if (page) {
            _stack.add(page);
            page->forceRedraw();
            draw();
        }
    }

    // JIT: Rimuove la pagina corrente e la DISTRUGGE (free RAM)
    void navigateBack() {
        if (_stack.size() > 1) {
            MenuPage* current = _stack[_stack.size() - 1];
            _stack.remove(_stack.size() - 1);
            delete current; // LIBERA MEMORIA QUI
            
            getCurrentPage()->forceRedraw();
            draw();
        }
    }

    MenuPage* getCurrentPage() {
        if (_stack.size() > 0) return _stack[_stack.size() - 1];
        return nullptr;
    }

    void handleInput(char key);

    void update() {
        MenuPage* current = getCurrentPage();
        if (current && current->needsRedraw()) {
            draw();
            current->clearRedraw();
        }
    }

    void drawIncrementalCursor(size_t oldIndex, size_t newIndex) {
        MenuPage* current = getCurrentPage();
        if (!current || !_initialized) return;

        size_t scroll_offset = current->_scroll_offset;
        
        if (oldIndex >= scroll_offset && oldIndex < scroll_offset + 3) {
            size_t row = oldIndex - scroll_offset + 1;
            LCD_set_cursor(0, row);
            LCD_write_str("                    ");
            current->getItem(oldIndex)->draw(row, false);
        }
        
        if (newIndex >= scroll_offset && newIndex < scroll_offset + 3) {
            size_t row = newIndex - scroll_offset + 1;
            LCD_set_cursor(0, row);
            LCD_write_str("                    ");
            current->getItem(newIndex)->draw(row, true);
        }
    }

    void draw() {
        MenuPage* current = getCurrentPage();
        if (!current || !_initialized) return;

        LCD_clear();
        LCD_set_cursor(0, 0);
        printLabel(current->_title); // Usa helper per Flash String
        
        size_t count = current->getItemsCount();
        size_t max_lines = 3;
        size_t scroll_offset = current->_scroll_offset;
        
        for (size_t i = 0; i < min(count - scroll_offset, max_lines); i++) {
            size_t itemIdx = i + scroll_offset;
            MenuItem* item = current->getItem(itemIdx);
            item->draw(i + 1, itemIdx == current->getSelectedIndex());
        }
        
        if (scroll_offset > 0) {
            LCD_set_cursor(19, 1);
            LCD_write_char('^');
        }
        if (scroll_offset + max_lines < count) {
            LCD_set_cursor(19, 3);
            LCD_write_char('v');
        }
    }
    
    // Helper per stampare stringhe Flash
    void printLabel(const __FlashStringHelper* f_str) {
        char buf[21];
        strncpy_P(buf, (const char*)f_str, 20);
        buf[20] = 0;
        LCD_write_str(buf);
    }
};

// FIX: DEFINIZIONI di MenuPage::handleInput e handleEvent DOPO NavigationManager
inline bool MenuPage::handleInput(char key) {
    size_t oldIndex = _selected_index;

    // FIX: Prima prova a passare l'input all'item corrente
    if (_selected_index < _items.size()) {
        bool handled = _items[_selected_index]->handleInput(key);
        if (handled) {
            _needs_redraw = true;
            return true; // FIX: Propaga il fatto che è stato gestito
        }
    }

    // Se l'item non ha gestito l'input, fai navigazione normale
    if (key == 'U') {
        if (_selected_index > 0) { 
            _selected_index--; 
            
            if (_selected_index < _scroll_offset) {
                _scroll_offset = _selected_index;
                _needs_redraw = true;
            } else {
                NavigationManager::instance().drawIncrementalCursor(oldIndex, _selected_index);
                return true; // FIX: Gestito dalla navigazione
            }
        }
        return true; // FIX: Gestito (anche se non c'è movimento)
    } else if (key == 'D') {
        if (_selected_index < static_cast<size_t>(_items.size() - 1)) {
            _selected_index++; 
            
            if (_selected_index >= _scroll_offset + 3) {
                _scroll_offset = _selected_index - 2;
                _needs_redraw = true;
            } else {
                NavigationManager::instance().drawIncrementalCursor(oldIndex, _selected_index);
                return true; // FIX: Gestito dalla navigazione
            }
        }
        return true; // FIX: Gestito (anche se non c'è movimento)
    }
    
    return false; // Altri tasti non gestiti
}

inline void MenuPage::handleEvent(EventType type, IDevice* device, int value) {
    MenuPage* currentPage = NavigationManager::instance().getCurrentPage();
    if (currentPage != this) return;
    
    for (size_t i = 0; i < _items.size(); i++) {
        if (_items[i]->relatesTo(device)) {
            _needs_redraw = true;
            return;
        }
    }
}

// --- ITEMS ---

class DeviceToggleItem : public MenuItem {
private:
    IDevice* _device;
public:
    DeviceToggleItem(IDevice* device) : _device(device) {}
    bool relatesTo(IDevice* dev) override { return _device == dev; }
    void draw(uint8_t row, bool selected) override {
        LCD_set_cursor(0, row);
        LCD_write_str(selected ? "> " : "  ");
        LCD_write_str(_device->name);
        LCD_set_cursor(15, row);
        
        if (_device->isLight()) {
            SimpleLight* light = static_cast<SimpleLight*>(_device);
            LCD_write_str(light->getState() ? "ON " : "OFF");
        }
    }
    bool handleInput(char key) override {
        if (key == 'E' && _device->isLight()) {
            static_cast<SimpleLight*>(_device)->toggle();
            return true;
        }
        return false;
    }
};

// Base class for slider rendering (Template Hoisting optimization)
class SliderRenderer {
protected:
    static inline bool _customCharsLoaded = false;
    
    static void loadCustomChars() {
        if (!_customCharsLoaded) {
            const uint8_t customChars[5][8] = {
                {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // Empty
                {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10}, // 1 pixel
                {0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18}, // 2 pixels
                {0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C}, // 3 pixels
                {0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E}  // 4 pixels
            };
            
            for (uint8_t i = 0; i < 5; i++) {
                LCDcreateChar(i, const_cast<uint8_t*>(customChars[i]));
            }
            _customCharsLoaded = true;
        }
    }
    
    // Extracted common progress bar drawing logic
    static void drawProgressBar(uint8_t row, uint8_t val, uint8_t minVal, uint8_t maxVal) {
        // Row 1: Full-width progress bar (20 chars = 100 pixels)
        LCD_set_cursor(0, row + 1);
        
        // Map value to pixels (0-100 pixels across 20 chars)
        uint16_t totalPixels = map(val, minVal, maxVal, 0, 100);
        uint8_t fullBlocks = totalPixels / 5;
        uint8_t partialPixels = totalPixels % 5;
        
        // Draw full blocks (0xFF = all 5 pixels filled)
        for (uint8_t i = 0; i < fullBlocks && i < 20; i++) {
            LCD_write_char(0xFF);
        }
        
        // Draw partial block
        if (fullBlocks < 20 && partialPixels > 0) {
            LCD_write_char(partialPixels - 1); // Custom char 0-4
            fullBlocks++;
        }
        
        // Fill remaining with empty spaces
        for (uint8_t i = fullBlocks; i < 20; i++) {
            LCD_write_char(' ');
        }
    }
};

// Template-based Value Slider with Pixel-Perfect UI
template<typename DeviceType>
class ValueSliderItem : public MenuItem, public SliderRenderer {
private:
    DeviceType* _device;
    const __FlashStringHelper* _label;
    uint8_t (DeviceType::*_getter)() const;
    void (DeviceType::*_setter)(uint8_t);
    uint8_t _min, _max, _step;

public:
    ValueSliderItem(DeviceType* device, 
                   const __FlashStringHelper* label,
                   uint8_t (DeviceType::*getter)() const,
                   void (DeviceType::*setter)(uint8_t),
                   uint8_t minVal, uint8_t maxVal, uint8_t step)
        : _device(device), _label(label), _getter(getter), _setter(setter),
          _min(minVal), _max(maxVal), _step(step) {
        loadCustomChars();
    }

    bool relatesTo(IDevice* dev) override { return _device == dev; }

    void draw(uint8_t row, bool selected) override {
        // Row 0: Label and value
        LCD_set_cursor(0, row);
        
        printLabel(_label);
        LCD_write_str(": ");
        
        uint8_t val = (_device->*_getter)();
        char buf[5];
        itoa(val, buf, 10);
        LCD_write_str(buf);
        
        // Clear rest of first row
        uint8_t cursor_pos = strlen_P((const char*)_label) + 2 + strlen(buf);
        while (cursor_pos < 20) {
            LCD_write_char(' ');
            cursor_pos++;
        }

        // Row 1: Progress bar (delegated to base class)
        drawProgressBar(row, val, _min, _max);
    }

    bool handleInput(char key) override {
        uint8_t current = (_device->*_getter)();
        
        if (key == 'U') {
            // Safely increment with clamping
            uint8_t newVal;
            if (current > _max - _step) {
                newVal = _max;
            } else {
                newVal = current + _step;
            }
            (_device->*_setter)(newVal);
            return true;
            
        } else if (key == 'D') {
            // Safely decrement with clamping
            uint8_t newVal;
            if (current < _min + _step) {
                newVal = _min;
            } else {
                newVal = current - _step;
            }
            (_device->*_setter)(newVal);
            return true;
        }
        
        return false;
    }
};

// Template helper function for automatic type deduction
template<typename DeviceType>
ValueSliderItem<DeviceType>* makeSlider(
    DeviceType* device,
    const __FlashStringHelper* label,
    uint8_t (DeviceType::*getter)() const,
    void (DeviceType::*setter)(uint8_t),
    uint8_t minVal = 0,
    uint8_t maxVal = 100,
    uint8_t step = 3
) {
    return new ValueSliderItem<DeviceType>(device, label, getter, setter, minVal, maxVal, step);
}

// Template-based Live Value Display Item
template<typename T>
class LiveItem : public MenuItem {
private:
    const __FlashStringHelper* _label;
    T* _object;
    int16_t (T::*_getter)() const;
    const __FlashStringHelper* _unit;
    bool _isTemperature;
    IDevice* _device;  // For relatesTo() when T is a sensor

    void printValue(int16_t value) {
        char buf[8];
        if (_isTemperature) {
            // Print as XX.X format (e.g., 205 -> "20.5")
            int16_t whole = value / 10;
            int16_t decimal = abs(value % 10);
            
            itoa(whole, buf, 10);
            LCD_write_str(buf);
            LCD_write_char('.');
            
            buf[0] = '0' + decimal;
            buf[1] = '\0';
            LCD_write_str(buf);
            LCD_write_char(0xDF);  // Degree symbol
        } else {
            itoa(value, buf, 10);
            LCD_write_str(buf);
            LCD_write_char(' ');
        }
        printLabel(_unit);
    }

public:
    // Constructor for stats (Min/Max/Avg)
    LiveItem(const __FlashStringHelper* label, T* object, int16_t (T::*getter)() const,
             const __FlashStringHelper* unit, bool isTemp = false)
        : _label(label), _object(object), _getter(getter), _unit(unit), 
          _isTemperature(isTemp), _device(nullptr) {}

    // Constructor for live sensor display
    LiveItem(IDevice* device, T* object, int16_t (T::*getter)() const,
             const __FlashStringHelper* unit, bool isTemp = false)
        : _label(nullptr), _object(object), _getter(getter), _unit(unit),
          _isTemperature(isTemp), _device(device) {}

    bool relatesTo(IDevice* dev) override { return _device == dev; }

    void draw(uint8_t row, bool selected) override {
        LCD_set_cursor(0, row);
        LCD_write_str(selected ? "> " : "  ");
        
        if (_label) {
            printLabel(_label);
        } else if (_device) {
            LCD_write_str(_device->name);
        }
        
        LCD_write_str(": ");
        
        // Call member function pointer to get value
        int16_t value = (_object->*_getter)();
        printValue(value);
    }

    bool handleInput(char key) override { return false; }
};

// Helper function for automatic type deduction
template<typename T>
LiveItem<T>* makeLiveItem(
    const __FlashStringHelper* label,
    T* object,
    int16_t (T::*getter)() const,
    const __FlashStringHelper* unit,
    bool isTemp = false
) {
    return new LiveItem<T>(label, object, getter, unit, isTemp);
}

// Overload for device-based live item
template<typename T>
LiveItem<T>* makeLiveItem(
    IDevice* device,
    T* object,
    int16_t (T::*getter)() const,
    const __FlashStringHelper* unit,
    bool isTemp = false
) {
    return new LiveItem<T>(device, object, getter, unit, isTemp);
}

// Specialized LiveItem for boolean PIR sensor
class LivePIRItem : public MenuItem {
private:
    PIRSensorDevice* _sensor;

public:
    LivePIRItem(PIRSensorDevice* sensor) : _sensor(sensor) {}

    bool relatesTo(IDevice* dev) override { return _sensor == dev; }

    void draw(uint8_t row, bool selected) override {
        LCD_set_cursor(0, row);
        LCD_write_str(selected ? "> " : "  ");
        LCD_write_str(_sensor->name);
        LCD_write_str(": ");
        LCD_write_str(_sensor->isMotionDetected() ? "Yes" : "No");
    }

    bool handleInput(char key) override { return false; }
};

// NUOVO: Item per calibrazione luce
class LightCalibrationItem : public MenuItem {
private:
    const __FlashStringHelper* _label;
    PhotoresistorSensor* _sensor;
    bool _isDark;  // true = calibra buio, false = calibra luce

public:
    LightCalibrationItem(const __FlashStringHelper* label, PhotoresistorSensor* sensor, bool isDark)
        : _label(label), _sensor(sensor), _isDark(isDark) {}

    void draw(uint8_t row, bool selected) override {
        LCD_set_cursor(0, row);
        LCD_write_str(selected ? "> " : "  ");
        printLabel(_label);
    }

    bool handleInput(char key) override {
        if (key == 'E') {
            if (_isDark) {
                _sensor->calibrateCurrentAsMin();
            } else {
                _sensor->calibrateCurrentAsMax();
            }
            // Forza aggiornamento immediato
            EventSystem::instance().emit(EventType::SensorUpdated, _sensor, _sensor->getValue());
            return true;
        }
        return false;
    }
};

// NUOVO: Item per eseguire un'azione e tornare indietro (per liste scrollabili)
class ActionItem : public MenuItem {
private:
    IDevice* _device;
    const char* _label;
    void (*_action)(IDevice*, int);
    int _param;
    NavigationManager& _nav;

public:
    ActionItem(const char* label, IDevice* device, void (*action)(IDevice*, int), int param, NavigationManager& nav)
        : _device(device), _label(label), _action(action), _param(param), _nav(nav) {}

    void draw(uint8_t row, bool selected) override {
        LCD_set_cursor(0, row);
        LCD_write_str(selected ? "> " : "  ");
        LCD_write_str(_label);
    }

    bool handleInput(char key) override {
        if (key == 'E') {
            _action(_device, _param);
            _nav.navigateBack();
            return true;
        }
        return false;
    }
};

// JIT: SubMenuItem ora usa un Builder invece di una pagina pre-allocata
class SubMenuItem : public MenuItem {
private:
    const __FlashStringHelper* _label;
    PageBuilder _builder;
    void* _context;
    NavigationManager& _nav;

public:
    SubMenuItem(const __FlashStringHelper* label, PageBuilder builder, void* context, NavigationManager& nav) 
        : _label(label), _builder(builder), _context(context), _nav(nav) {}

    MenuItemType getType() const override { return MenuItemType::SUBMENU; }
    
    // Metodo per creare la pagina on-the-fly
    MenuPage* createPage() const { return _builder(_context); }

    void draw(uint8_t row, bool selected) override {
        LCD_set_cursor(0, row);
        LCD_write_str(selected ? "> " : "  ");
        printLabel(_label);
        LCD_write_str(" >");
    }

    bool handleInput(char key) override {
        if (key == 'E') {
            MenuPage* newPage = createPage();
            _nav.pushPage(newPage);
            return true;
        }
        return false;
    }
};

class BackMenuItem : public MenuItem {
private:
    NavigationManager& _nav;
public:
    BackMenuItem(NavigationManager& nav) : _nav(nav) {}
    void draw(uint8_t row, bool selected) override {
        LCD_set_cursor(0, row);
        LCD_write_str(selected ? "> " : "  ");
        LCD_write_str("<< Back");
    }
    bool handleInput(char key) override {
        if (key == 'E') {
            _nav.navigateBack();
            return true;
        }
        return false;
    }
};

// --- BUILDERS (Refactored without static wrappers) ---

class MenuBuilder {
private:
    // Helper action functions for ActionItem (minimal wrappers)
    static void setOutsideModeAction(IDevice* d, int v) {
        static_cast<OutsideLight*>(d)->setMode((OutsideMode)v);
    }
    
    static void setRGBPresetAction(IDevice* d, int v) {
        static_cast<RGBLight*>(d)->setPreset((RGBPreset)v);
    }

public:
    // Simplified slider page builders using makeSlider
    static MenuPage* buildRedPage(void* context) {
        RGBLight* light = static_cast<RGBLight*>(context);
        MenuPage* page = new MenuPage(F("Red Channel"), nullptr);
        if (!page) return nullptr;
        
        page->addItem(makeSlider(light, F("Red"), &RGBLight::getRed, &RGBLight::setRed, 0, 255, 3));
        page->addItem(new BackMenuItem(NavigationManager::instance()));
        return page;
    }

    static MenuPage* buildGreenPage(void* context) {
        RGBLight* light = static_cast<RGBLight*>(context);
        MenuPage* page = new MenuPage(F("Green Channel"), nullptr);
        if (!page) return nullptr;
        
        page->addItem(makeSlider(light, F("Green"), &RGBLight::getGreen, &RGBLight::setGreen, 0, 255, 3));
        page->addItem(new BackMenuItem(NavigationManager::instance()));
        return page;
    }

    static MenuPage* buildBluePage(void* context) {
        RGBLight* light = static_cast<RGBLight*>(context);
        MenuPage* page = new MenuPage(F("Blue Channel"), nullptr);
        if (!page) return nullptr;
        
        page->addItem(makeSlider(light, F("Blue"), &RGBLight::getBlue, &RGBLight::setBlue, 0, 255, 3));
        page->addItem(new BackMenuItem(NavigationManager::instance()));
        return page;
    }

    static MenuPage* buildBrightnessPage(void* context) {
        DimmableLight* light = static_cast<DimmableLight*>(context);
        MenuPage* page = new MenuPage(F("Brightness"), nullptr);
        if (!page) return nullptr;
        
        page->addItem(makeSlider(light, F("Level"), &DimmableLight::getBrightness, &DimmableLight::setBrightness, 0, 100, 2));
        return page;
    }

    static MenuPage* buildCustomColorPage(void* context) {
        RGBLight* light = static_cast<RGBLight*>(context);
        MenuPage* page = new MenuPage(F("Custom Color"), nullptr);
        if (!page) return nullptr;

        page->addItem(new SubMenuItem(F("Set Red"), buildRedPage, light, NavigationManager::instance()));
        page->addItem(new SubMenuItem(F("Set Green"), buildGreenPage, light, NavigationManager::instance()));
        page->addItem(new SubMenuItem(F("Set Blue"), buildBluePage, light, NavigationManager::instance()));
        page->addItem(new BackMenuItem(NavigationManager::instance()));

        return page;
    }

    static MenuPage* buildRGBPresetsPage(void* context) {
        RGBLight* light = static_cast<RGBLight*>(context);
        MenuPage* page = new MenuPage(F("Select Preset"), NavigationManager::instance().getCurrentPage());
        page->addItem(new ActionItem("Warm White", light, setRGBPresetAction, (int)RGBPreset::WARM_WHITE, NavigationManager::instance()));
        page->addItem(new ActionItem("Cool White", light, setRGBPresetAction, (int)RGBPreset::COOL_WHITE, NavigationManager::instance()));
        page->addItem(new ActionItem("Red", light, setRGBPresetAction, (int)RGBPreset::RED, NavigationManager::instance()));
        page->addItem(new ActionItem("Green", light, setRGBPresetAction, (int)RGBPreset::GREEN, NavigationManager::instance()));
        page->addItem(new ActionItem("Blue", light, setRGBPresetAction, (int)RGBPreset::BLUE, NavigationManager::instance()));
        page->addItem(new ActionItem("Ocean", light, setRGBPresetAction, (int)RGBPreset::OCEAN, NavigationManager::instance()));
        page->addItem(new BackMenuItem(NavigationManager::instance()));
        return page;
    }

    static MenuPage* buildRGBLightPage(void* context) {
        RGBLight* light = static_cast<RGBLight*>(context);
        MenuPage* page = new MenuPage(F("RGB Light"), NavigationManager::instance().getCurrentPage());
        page->addItem(new DeviceToggleItem(light));
        page->addItem(new SubMenuItem(F("Set Brightness"), buildBrightnessPage, light, NavigationManager::instance()));
        page->addItem(new SubMenuItem(F("Color Presets"), buildRGBPresetsPage, light, NavigationManager::instance()));
        page->addItem(new SubMenuItem(F("Custom Color"), buildCustomColorPage, light, NavigationManager::instance()));
        page->addItem(new BackMenuItem(NavigationManager::instance()));
        return page;
    }

    static MenuPage* buildDimmableLightPage(void* context) {
        DimmableLight* light = static_cast<DimmableLight*>(context);
        MenuPage* page = new MenuPage(F("Dimmable Light"), NavigationManager::instance().getCurrentPage());
        page->addItem(new DeviceToggleItem(light));
        page->addItem(new SubMenuItem(F("Set Brightness"), buildBrightnessPage, light, NavigationManager::instance()));
        page->addItem(new BackMenuItem(NavigationManager::instance()));
        return page;
    }

    static MenuPage* buildOutsideModesPage(void* context) {
        OutsideLight* light = static_cast<OutsideLight*>(context);
        MenuPage* page = new MenuPage(F("Select Mode"), NavigationManager::instance().getCurrentPage());
        page->addItem(new ActionItem("OFF", light, setOutsideModeAction, (int)OutsideMode::OFF, NavigationManager::instance()));
        page->addItem(new ActionItem("ON", light, setOutsideModeAction, (int)OutsideMode::ON, NavigationManager::instance()));
        page->addItem(new ActionItem("AUTO LIGHT", light, setOutsideModeAction, (int)OutsideMode::AUTO_LIGHT, NavigationManager::instance()));
        page->addItem(new ActionItem("AUTO MOTION", light, setOutsideModeAction, (int)OutsideMode::AUTO_MOTION, NavigationManager::instance()));
        page->addItem(new BackMenuItem(NavigationManager::instance()));
        return page;
    }

    static MenuPage* buildOutsideLightPage(void* context) {
        OutsideLight* light = (OutsideLight*)context;
        MenuPage* page = new MenuPage(F("Outside Light"), NavigationManager::instance().getCurrentPage());
        page->addItem(new SubMenuItem(F("Set Mode"), buildOutsideModesPage, light, NavigationManager::instance()));
        page->addItem(new BackMenuItem(NavigationManager::instance()));
        return page;
    }

    static MenuPage* buildLightsPage(void* context) {
        MenuPage* page = new MenuPage(F("Lights"), NavigationManager::instance().getCurrentPage());
        DeviceRegistry& registry = DeviceRegistry::instance();
        DynamicArray<IDevice*>& devices = registry.getDevices();
        
        for (size_t i = 0; i < devices.size(); i++) {
            IDevice* d = devices[i];
            if (d->type == DeviceType::LightSimple) {
                page->addItem(new DeviceToggleItem(d));
            } else if (d->type == DeviceType::LightDimmable) {
                page->addItem(new SubMenuItem(F("Dimmable"), buildDimmableLightPage, d, NavigationManager::instance()));
            } else if (d->type == DeviceType::LightRGB) {
                page->addItem(new SubMenuItem(F("RGB Light"), buildRGBLightPage, d, NavigationManager::instance()));
            } else if (d->type == DeviceType::LightOutside) {
                page->addItem(new SubMenuItem(F("Outside Light"), buildOutsideLightPage, d, NavigationManager::instance()));
            }
        }
        page->addItem(new BackMenuItem(NavigationManager::instance()));
        return page;
    }

    // REFACTORED: Template-based sensor stats page
    static MenuPage* buildSensorStatsPage(void* context) {
        IDevice* device = static_cast<IDevice*>(context);
        MenuPage* page = new MenuPage(F("Statistics"), NavigationManager::instance().getCurrentPage());
        
        if (device->type == DeviceType::SensorTemperature) {
            TemperatureSensor* temp = static_cast<TemperatureSensor*>(device);
            SensorStats* stats = &temp->getStats();
            
            // Live current temperature
            page->addItem(makeLiveItem(device, temp, &TemperatureSensor::getTemperature, F("C"), true));
            
            // Statistics using template
            page->addItem(makeLiveItem(F("Min"), stats, &SensorStats::getMin, F("C"), true));
            page->addItem(makeLiveItem(F("Max"), stats, &SensorStats::getMax, F("C"), true));
            page->addItem(makeLiveItem(F("Avg"), stats, &SensorStats::getAverage, F("C"), true));
            
        } else if (device->type == DeviceType::SensorLight) {
            PhotoresistorSensor* light = static_cast<PhotoresistorSensor*>(device);
            SensorStats* stats = &light->getStats();
            
            // Live current light level (returns int16_t via getValue)
            page->addItem(makeLiveItem(device, light, 
                static_cast<int16_t (PhotoresistorSensor::*)() const>(&PhotoresistorSensor::getValue), 
                F("%"), false));
            
            // Statistics
            page->addItem(makeLiveItem(F("Min"), stats, &SensorStats::getMin, F("%"), false));
            page->addItem(makeLiveItem(F("Max"), stats, &SensorStats::getMax, F("%"), false));
            page->addItem(makeLiveItem(F("Avg"), stats, &SensorStats::getAverage, F("%"), false));
        }
        
        page->addItem(new BackMenuItem(NavigationManager::instance()));
        return page;
    }

    // REFACTORED: Light settings with template
    static MenuPage* buildLightSettingsPage(void* context) {
        PhotoresistorSensor* light = static_cast<PhotoresistorSensor*>(context);
        MenuPage* page = new MenuPage(F("Light Settings"), NavigationManager::instance().getCurrentPage());
        
        // Sottomenu statistiche
        page->addItem(new SubMenuItem(F("View Stats"), buildSensorStatsPage, light, NavigationManager::instance()));
        
        // Calibrazione
        page->addItem(new LightCalibrationItem(F("Set Dark Limit"), light, true));
        page->addItem(new LightCalibrationItem(F("Set Bright Limit"), light, false));
        
        page->addItem(new BackMenuItem(NavigationManager::instance()));
        return page;
    }

    static MenuPage* buildSensorsPage(void* context) {
        MenuPage* page = new MenuPage(F("Sensors"), NavigationManager::instance().getCurrentPage());
        DeviceRegistry& registry = DeviceRegistry::instance();
        DynamicArray<IDevice*>& devices = registry.getDevices();
        
        for (size_t i = 0; i < devices.size(); i++) {
            IDevice* d = devices[i];
            if (d->isSensor()) {
                if (d->type == DeviceType::SensorTemperature) {
                    page->addItem(new SubMenuItem(F("Temperature"), buildSensorStatsPage, d, NavigationManager::instance()));
                    
                } else if (d->type == DeviceType::SensorLight) {
                    page->addItem(new SubMenuItem(F("Light Sensor"), buildLightSettingsPage, d, NavigationManager::instance()));
                    
                } else if (d->type == DeviceType::SensorPIR) {
                    // Use specialized PIR item
                    page->addItem(new LivePIRItem(static_cast<PIRSensorDevice*>(d)));
                }
            }
        }
        
        page->addItem(new BackMenuItem(NavigationManager::instance()));
        return page;
    }

    static MenuPage* buildMainMenu() {
        // Root page è l'unica creata staticamente all'inizio
        MenuPage* root = new MenuPage(F("Main Menu"));
        root->addItem(new SubMenuItem(F("Lights"), buildLightsPage, nullptr, NavigationManager::instance()));
        root->addItem(new SubMenuItem(F("Sensors"), buildSensorsPage, nullptr, NavigationManager::instance()));
        return root;
    }
};

// Definizione di NavigationManager::handleInput
inline void NavigationManager::handleInput(char key) {
    MenuPage* current = getCurrentPage();
    if (!current) return;
    
    if (key == 'B') {
        navigateBack();
    } else if (key == 'E') {
        size_t idx = current->getSelectedIndex();
        if (idx < current->getItemsCount()) {
            MenuItem* item = current->getItem(idx);
            if (item->getType() == MenuItemType::SUBMENU) {
                SubMenuItem* sub = static_cast<SubMenuItem*>(item);
                MenuPage* newPage = sub->createPage();
                pushPage(newPage);
            } else {
                item->handleInput(key);
            }
        }
    } else {
        current->handleInput(key);
    }
    
    if (getCurrentPage()) getCurrentPage()->forceRedraw();
}

#endif