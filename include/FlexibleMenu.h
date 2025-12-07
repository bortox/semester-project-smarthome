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
        
        if (_device->type == DeviceType::LightSimple || _device->type == DeviceType::LightOutside) {
            bool state = static_cast<SimpleLight*>(_device)->getState();
            LCD_write_str(state ? "ON " : "OFF");
        } else if (_device->type == DeviceType::LightDimmable || _device->type == DeviceType::LightRGB) {
            bool state = static_cast<SimpleLight*>(_device)->getState();
            LCD_write_str(state ? "ON " : "OFF");
        }
    }
    bool handleInput(char key) override {
        if (key == 'E') {
            if (_device->isLight()) static_cast<SimpleLight*>(_device)->toggle();
            return true;
        }
        return false;
    }
};

// DRY: Template-based Value Slider
template<typename DeviceType>
class ValueSliderItem : public MenuItem {
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
          _min(minVal), _max(maxVal), _step(step) {}

    bool relatesTo(IDevice* dev) override { return _device == dev; }

    void draw(uint8_t row, bool selected) override {
        LCD_set_cursor(0, row);
        LCD_write_str(selected ? "< " : "  ");
        
        printLabel(_label);
        LCD_write_str(": ");
        
        uint8_t val = (_device->*_getter)();
        char buf[5];
        itoa(val, buf, 10);
        LCD_write_str(buf);
        
        if (selected) LCD_write_str(" >");

        LCD_set_cursor(0, row + 1);
        LCD_write_char('[');
        int bars = map(val, _min, _max, 0, 18);
        for (int i = 0; i < 18; i++) {
            LCD_write_char(i < bars ? (char)0xFF : ' ');
        }
        LCD_write_char(']');
    }

    bool handleInput(char key) override {
        uint8_t current = (_device->*_getter)();
        if (key == 'U') {
            uint8_t newVal = min(_max, (uint8_t)(current + _step));
            (_device->*_setter)(newVal);
            return true;
        } else if (key == 'D') {
            uint8_t newVal = max(_min, (uint8_t)(current - _step));
            (_device->*_setter)(newVal);
            return true;
        }
        return false;
    }
};

// FIX: Non-template wrapper for function pointer compatibility
class GenericSliderItem : public MenuItem {
private:
    IDevice* _device;
    const __FlashStringHelper* _label;
    ValueGetter _getter;
    ValueSetter _setter;
    int _min, _max, _step;

public:
    GenericSliderItem(IDevice* device, 
                     const __FlashStringHelper* label,
                     ValueGetter getter, ValueSetter setter,
                     int minVal, int maxVal, int step)
        : _device(device), _label(label), _getter(getter), _setter(setter),
          _min(minVal), _max(maxVal), _step(step) {}

    bool relatesTo(IDevice* dev) override { return _device == dev; }

    void draw(uint8_t row, bool selected) override {
        LCD_set_cursor(0, row);
        LCD_write_str(selected ? "< " : "  ");
        
        printLabel(_label);
        LCD_write_str(": ");
        
        int val = _getter(_device);
        char buf[5];
        itoa(val, buf, 10);
        LCD_write_str(buf);
        
        if (selected) LCD_write_str(" >");

        LCD_set_cursor(0, row + 1);
        LCD_write_char('[');
        int bars = map(val, _min, _max, 0, 18);
        for (int i = 0; i < 18; i++) {
            LCD_write_char(i < bars ? (char)0xFF : ' ');
        }
        LCD_write_char(']');
    }

    bool handleInput(char key) override {
        int current = _getter(_device);
        if (key == 'U') {
            int newVal = min(_max, current + _step);
            _setter(_device, newVal);
            return true;
        } else if (key == 'D') {
            int newVal = max(_min, current - _step);
            _setter(_device, newVal);
            return true;
        }
        return false;
    }
};

// KISS: Merged InfoItem
class InfoItem : public MenuItem {
public:
    enum class Mode : uint8_t { STATIC, LIVE_TEMP, LIVE_LIGHT, LIVE_PIR };
    
private:
    const __FlashStringHelper* _label;
    Mode _mode;
    IDevice* _device;
    int16_t _staticValue;
    const __FlashStringHelper* _unit;
    bool _isTemperature;

    void printValue(int16_t value) {
        char buf[8];
        if (_isTemperature) {
            float val = value / 10.0f;
            dtostrf(val, 4, 1, buf);
            LCD_write_str(buf);
            LCD_write_char(0xDF);
        } else {
            itoa(value, buf, 10);
            LCD_write_str(buf);
            LCD_write_char(' ');
        }
        printLabel(_unit);
    }

public:
    InfoItem(const __FlashStringHelper* label, int16_t value, 
             const __FlashStringHelper* unit, bool isTemp = false)
        : _label(label), _mode(Mode::STATIC), _device(nullptr), 
          _staticValue(value), _unit(unit), _isTemperature(isTemp) {}

    InfoItem(IDevice* device, const __FlashStringHelper* unit, Mode mode)
        : _label(nullptr), _mode(mode), _device(device), _staticValue(0), 
          _unit(unit), _isTemperature(mode == Mode::LIVE_TEMP) {}

    bool relatesTo(IDevice* dev) override { return _device == dev; }

    void draw(uint8_t row, bool selected) override {
        LCD_set_cursor(0, row);
        LCD_write_str(selected ? "> " : "  ");
        
        if (_mode == Mode::STATIC) {
            printLabel(_label);
            LCD_write_str(": ");
            printValue(_staticValue);
        } else {
            if (_label) {
                printLabel(_label);
            } else {
                LCD_write_str(_device->name);
            }
            LCD_write_str(": ");
            
            switch (_mode) {
                case Mode::LIVE_TEMP: {
                    int16_t value = (int16_t)(static_cast<TemperatureSensor*>(_device)->getTemperature() * 10);
                    printValue(value);
                    break;
                }
                case Mode::LIVE_LIGHT: {
                    int16_t value = static_cast<PhotoresistorSensor*>(_device)->getValue();
                    printValue(value);
                    break;
                }
                case Mode::LIVE_PIR:
                    LCD_write_str(static_cast<PIRSensorDevice*>(_device)->isMotionDetected() ? "Yes" : "No");
                    break;
                case Mode::STATIC:
                    // Already handled above
                    break;
            }
        }
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
    ValueSetter _action;
    int _param;
    NavigationManager& _nav;

public:
    ActionItem(const char* label, IDevice* device, ValueSetter action, int param, NavigationManager& nav)
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

// --- BUILDERS (DRY Refactored) ---

class MenuBuilder {
public:
    // DRY: RGB Helper functions (since we can't modify Devices.h)
    static int getRed(IDevice* d) { return static_cast<RGBLight*>(d)->getColor().r; }
    static void setRedValue(IDevice* d, int v) {
        RGBLight* l = static_cast<RGBLight*>(d); 
        RGBColor c = l->getColor(); 
        c.r = v; 
        l->setColor(c); 
    }
    
    static int getGreen(IDevice* d) { return static_cast<RGBLight*>(d)->getColor().g; }
    static void setGreenValue(IDevice* d, int v) {
        RGBLight* l = static_cast<RGBLight*>(d); 
        RGBColor c = l->getColor(); 
        c.g = v; 
        l->setColor(c); 
    }
    
    static int getBlue(IDevice* d) { return static_cast<RGBLight*>(d)->getColor().b; }
    static void setBlueValue(IDevice* d, int v) {
        RGBLight* l = static_cast<RGBLight*>(d); 
        RGBColor c = l->getColor(); 
        c.b = v; 
        l->setColor(c); 
    }
    
    static int getBrightness(IDevice* d) { return static_cast<DimmableLight*>(d)->getBrightness(); }
    static void setBrightness(IDevice* d, int v) { static_cast<DimmableLight*>(d)->setBrightness(v); }
    
    // Helper Setters for ActionItem
    static void setOutsideMode(IDevice* d, int v) { static_cast<OutsideLight*>(d)->setMode((OutsideMode)v); }
    static void setRGBPreset(IDevice* d, int v) { static_cast<RGBLight*>(d)->setPreset((RGBPreset)v); }

    // DRY: Generic slider page (works with function pointers)
    static MenuPage* buildSliderPage(void* context, const __FlashStringHelper* title,
                                     const __FlashStringHelper* label,
                                     ValueGetter getter, ValueSetter setter,
                                     int minVal, int maxVal, int step) {
        IDevice* device = static_cast<IDevice*>(context);
        MenuPage* page = new MenuPage(title, nullptr);
        if (!page) return nullptr;

        // FIX: Use GenericSliderItem instead of template version
        page->addItem(new GenericSliderItem(device, label, getter, setter, minVal, maxVal, step));
        page->addItem(new BackMenuItem(NavigationManager::instance()));
        return page;
    }

    static MenuPage* buildRedPage(void* context) {
        return buildSliderPage(context, F("Red Channel"), F("Red"), getRed, setRedValue, 0, 255, 10);
    }

    static MenuPage* buildGreenPage(void* context) {
        return buildSliderPage(context, F("Green Channel"), F("Green"), getGreen, setGreenValue, 0, 255, 10);
    }

    static MenuPage* buildBluePage(void* context) {
        return buildSliderPage(context, F("Blue Channel"), F("Blue"), getBlue, setBlueValue, 0, 255, 10);
    }

    static MenuPage* buildBrightnessPage(void* context) {
        return buildSliderPage(context, F("Brightness"), F("Level"), getBrightness, setBrightness, 0, 100, 10);
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
        RGBLight* light = (RGBLight*)context;
        MenuPage* page = new MenuPage(F("Select Preset"), NavigationManager::instance().getCurrentPage());
        page->addItem(new ActionItem("Warm White", light, setRGBPreset, (int)RGBPreset::WARM_WHITE, NavigationManager::instance()));
        page->addItem(new ActionItem("Cool White", light, setRGBPreset, (int)RGBPreset::COOL_WHITE, NavigationManager::instance()));
        page->addItem(new ActionItem("Red", light, setRGBPreset, (int)RGBPreset::RED, NavigationManager::instance()));
        page->addItem(new ActionItem("Green", light, setRGBPreset, (int)RGBPreset::GREEN, NavigationManager::instance()));
        page->addItem(new ActionItem("Blue", light, setRGBPreset, (int)RGBPreset::BLUE, NavigationManager::instance()));
        page->addItem(new ActionItem("Ocean", light, setRGBPreset, (int)RGBPreset::OCEAN, NavigationManager::instance()));
        page->addItem(new BackMenuItem(NavigationManager::instance()));
        return page;
    }

    static MenuPage* buildRGBLightPage(void* context) {
        RGBLight* light = (RGBLight*)context;
        MenuPage* page = new MenuPage(F("RGB Light"), NavigationManager::instance().getCurrentPage());
        page->addItem(new DeviceToggleItem(light));
        page->addItem(new SubMenuItem(F("Set Brightness"), buildBrightnessPage, light, NavigationManager::instance()));
        page->addItem(new SubMenuItem(F("Color Presets"), buildRGBPresetsPage, light, NavigationManager::instance()));
        page->addItem(new SubMenuItem(F("Custom Color"), buildCustomColorPage, light, NavigationManager::instance()));
        page->addItem(new BackMenuItem(NavigationManager::instance()));
        return page;
    }

    static MenuPage* buildDimmableLightPage(void* context) {
        DimmableLight* light = (DimmableLight*)context;
        MenuPage* page = new MenuPage(F("Dimmable Light"), NavigationManager::instance().getCurrentPage());
        page->addItem(new DeviceToggleItem(light));
        page->addItem(new SubMenuItem(F("Set Brightness"), buildBrightnessPage, light, NavigationManager::instance()));
        page->addItem(new BackMenuItem(NavigationManager::instance()));
        return page;
    }

    static MenuPage* buildOutsideModesPage(void* context) {
        OutsideLight* light = (OutsideLight*)context;
        MenuPage* page = new MenuPage(F("Select Mode"), NavigationManager::instance().getCurrentPage());
        page->addItem(new ActionItem("OFF", light, setOutsideMode, (int)OutsideMode::OFF, NavigationManager::instance()));
        page->addItem(new ActionItem("ON", light, setOutsideMode, (int)OutsideMode::ON, NavigationManager::instance()));
        page->addItem(new ActionItem("AUTO LIGHT", light, setOutsideMode, (int)OutsideMode::AUTO_LIGHT, NavigationManager::instance()));
        page->addItem(new ActionItem("AUTO MOTION", light, setOutsideMode, (int)OutsideMode::AUTO_MOTION, NavigationManager::instance()));
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

    // KISS: Refactored using InfoItem
    static MenuPage* buildSensorStatsPage(void* context) {
        IDevice* device = static_cast<IDevice*>(context);
        MenuPage* page = new MenuPage(F("Statistics"), NavigationManager::instance().getCurrentPage());
        
        if (device->type == DeviceType::SensorTemperature) {
            TemperatureSensor* temp = static_cast<TemperatureSensor*>(device);
            SensorStats& stats = temp->getStats();
            
            page->addItem(new InfoItem(device, F("C"), InfoItem::Mode::LIVE_TEMP));
            page->addItem(new InfoItem(F("Min"), stats.getMin(), F("C"), true));
            page->addItem(new InfoItem(F("Max"), stats.getMax(), F("C"), true));
            page->addItem(new InfoItem(F("Avg"), stats.getAverage(), F("C"), true));
            
        } else if (device->type == DeviceType::SensorLight) {
            PhotoresistorSensor* light = static_cast<PhotoresistorSensor*>(device);
            SensorStats& stats = light->getStats();
            
            page->addItem(new InfoItem(device, F("%"), InfoItem::Mode::LIVE_LIGHT));
            page->addItem(new InfoItem(F("Min"), stats.getMin(), F("%"), false));
            page->addItem(new InfoItem(F("Max"), stats.getMax(), F("%"), false));
            page->addItem(new InfoItem(F("Avg"), stats.getAverage(), F("%"), false));
        }
        
        page->addItem(new BackMenuItem(NavigationManager::instance()));
        return page;
    }

    // NUOVO: Builder per pagina impostazioni luce (Stats + Calibrazione)
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
                    // KISS: Reuse InfoItem for PIR display
                    page->addItem(new InfoItem(d, F(""), InfoItem::Mode::LIVE_PIR));
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