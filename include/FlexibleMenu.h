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
        LCD_write_str(_device->getName());
        LCD_set_cursor(15, row);
        
        if (_device->getType() == DeviceType::LightSimple || _device->getType() == DeviceType::LightOutside) {
            bool state = static_cast<SimpleLight*>(_device)->getState();
            LCD_write_str(state ? "ON " : "OFF");
        } else if (_device->getType() == DeviceType::LightDimmable || _device->getType() == DeviceType::LightRGB) {
            // Per luci complesse mostra stato base
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

// NUOVO: Slider Generico per Luminosità e Colori
class ValueSliderItem : public MenuItem {
private:
    IDevice* _device;
    const __FlashStringHelper* _label;
    ValueGetter _getter;
    ValueSetter _setter;
    int _min, _max, _step;

public:
    ValueSliderItem(IDevice* device, const __FlashStringHelper* label, 
                   ValueGetter getter, ValueSetter setter, 
                   int minVal, int maxVal, int step)
        : _device(device), _label(label), _getter(getter), _setter(setter),
          _min(minVal), _max(maxVal), _step(step) {}

    bool relatesTo(IDevice* dev) override { return _device == dev; }

    void draw(uint8_t row, bool selected) override {
        LCD_set_cursor(0, row);
        if (selected) LCD_write_str("< ");
        else LCD_write_str("  ");
        
        printLabel(_label);
        LCD_write_str(": ");
        
        int val = _getter(_device);
        char buf[5];
        itoa(val, buf, 10);
        LCD_write_str(buf);
        
        if (selected) LCD_write_str(" >");

        // Barra progresso
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
            return true; // FIX: Consuma l'input
        } else if (key == 'D') {
            int newVal = max(_min, current - _step);
            _setter(_device, newVal);
            return true; // FIX: Consuma l'input
        }
        return false;
    }
};

// NUOVO: Item per visualizzare sensori
class SensorDisplayItem : public MenuItem {
private:
    IDevice* _device;
    const __FlashStringHelper* _unit;

public:
    SensorDisplayItem(IDevice* device, const __FlashStringHelper* unit) 
        : _device(device), _unit(unit) {}

    bool relatesTo(IDevice* dev) override { return _device == dev; }

    void draw(uint8_t row, bool selected) override {
        LCD_set_cursor(0, row);
        LCD_write_str(selected ? "> " : "  ");
        LCD_write_str(_device->getName());
        LCD_write_str(": ");
        
        // Ottieni valore in base al tipo
        if (_device->getType() == DeviceType::SensorTemperature) {
            TemperatureSensor* temp = static_cast<TemperatureSensor*>(_device);
            char buf[8];
            dtostrf(temp->getTemperature(), 4, 1, buf);
            LCD_write_str(buf);
        } else if (_device->getType() == DeviceType::SensorLight) {
            PhotoresistorSensor* light = static_cast<PhotoresistorSensor*>(_device);
            char buf[5];
            itoa(light->getValue(), buf, 10);
            LCD_write_str(buf);
        }
        
        LCD_write_char(' ');
        printLabel(_unit);
    }

    bool handleInput(char key) override {
        return false; // Non consuma input, permette navigazione
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

// --- BUILDERS ---

class MenuBuilder {
public:
    // Helper Getters/Setters per ValueSliderItem
    static int getBrightness(IDevice* d) { return static_cast<DimmableLight*>(d)->getBrightness(); }
    static void setBrightness(IDevice* d, int v) { static_cast<DimmableLight*>(d)->setBrightness(v); }
    
    static int getRed(IDevice* d) { return static_cast<RGBLight*>(d)->getColor().r; }
    static void setRed(IDevice* d, int v) { 
        RGBLight* l = static_cast<RGBLight*>(d); 
        RGBColor c = l->getColor(); c.r = v; l->setColor(c); 
    }
    static int getGreen(IDevice* d) { return static_cast<RGBLight*>(d)->getColor().g; }
    static void setGreen(IDevice* d, int v) { 
        RGBLight* l = static_cast<RGBLight*>(d); 
        RGBColor c = l->getColor(); c.g = v; l->setColor(c); 
    }
    static int getBlue(IDevice* d) { return static_cast<RGBLight*>(d)->getColor().b; }
    static void setBlue(IDevice* d, int v) { 
        RGBLight* l = static_cast<RGBLight*>(d); 
        RGBColor c = l->getColor(); c.b = v; l->setColor(c); 
    }

    // Helper Setters per ActionItem
    static void setOutsideMode(IDevice* d, int v) { static_cast<OutsideLight*>(d)->setMode((OutsideMode)v); }
    static void setRGBPreset(IDevice* d, int v) { static_cast<RGBLight*>(d)->setPreset((RGBPreset)v); }

    // --- PAGE BUILDERS (JIT) ---

    static MenuPage* buildBrightnessPage(void* context) {
        DimmableLight* light = (DimmableLight*)context;
        MenuPage* page = new MenuPage(F("Brightness"), NavigationManager::instance().getCurrentPage());
        page->addItem(new ValueSliderItem(light, F("Level"), getBrightness, setBrightness, 0, 100, 10));
        page->addItem(new BackMenuItem(NavigationManager::instance()));
        return page;
    }

    static MenuPage* buildRGBChannelPage(void* context) {
        // Context è una struct o array? Semplifichiamo: context è RGBLight, ma quale canale?
        // Per semplicità, creiamo 3 builder separati o usiamo una struct context.
        // Qui uso builder separati per R, G, B per evitare allocazioni extra di context struct.
        return nullptr; // Placeholder
    }
    
    static MenuPage* buildRedPage(void* context) {
        RGBLight* light = (RGBLight*)context;
        MenuPage* page = new MenuPage(F("Red Channel"), NavigationManager::instance().getCurrentPage());
        page->addItem(new ValueSliderItem(light, F("Red"), getRed, setRed, 0, 255, 10));
        page->addItem(new BackMenuItem(NavigationManager::instance()));
        return page;
    }
    static MenuPage* buildGreenPage(void* context) {
        RGBLight* light = (RGBLight*)context;
        MenuPage* page = new MenuPage(F("Green Channel"), NavigationManager::instance().getCurrentPage());
        page->addItem(new ValueSliderItem(light, F("Green"), getGreen, setGreen, 0, 255, 10));
        page->addItem(new BackMenuItem(NavigationManager::instance()));
        return page;
    }
    static MenuPage* buildBluePage(void* context) {
        RGBLight* light = (RGBLight*)context;
        MenuPage* page = new MenuPage(F("Blue Channel"), NavigationManager::instance().getCurrentPage());
        page->addItem(new ValueSliderItem(light, F("Blue"), getBlue, setBlue, 0, 255, 10));
        page->addItem(new BackMenuItem(NavigationManager::instance()));
        return page;
    }

    static MenuPage* buildCustomColorPage(void* context) {
        RGBLight* light = (RGBLight*)context;
        MenuPage* page = new MenuPage(F("Custom Color"), NavigationManager::instance().getCurrentPage());
        page->addItem(new SubMenuItem(F("Set Red"), buildRedPage, light, NavigationManager::instance()));
        page->addItem(new SubMenuItem(F("Set Green"), buildGreenPage, light, NavigationManager::instance()));
        page->addItem(new SubMenuItem(F("Set Blue"), buildBluePage, light, NavigationManager::instance()));
        page->addItem(new BackMenuItem(NavigationManager::instance()));
        return page;
    }

    static MenuPage* buildRGBPresetsPage(void* context) {
        RGBLight* light = (RGBLight*)context;
        MenuPage* page = new MenuPage(F("Select Preset"), NavigationManager::instance().getCurrentPage());
        // Lista scrollabile di preset
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
            if (d->getType() == DeviceType::LightSimple) {
                page->addItem(new DeviceToggleItem(d));
            } else if (d->getType() == DeviceType::LightDimmable) {
                page->addItem(new SubMenuItem(F("Dimmable"), buildDimmableLightPage, d, NavigationManager::instance()));
            } else if (d->getType() == DeviceType::LightRGB) {
                page->addItem(new SubMenuItem(F("RGB Light"), buildRGBLightPage, d, NavigationManager::instance()));
            } else if (d->getType() == DeviceType::LightOutside) {
                page->addItem(new SubMenuItem(F("Outside Light"), buildOutsideLightPage, d, NavigationManager::instance()));
            }
        }
        page->addItem(new BackMenuItem(NavigationManager::instance()));
        return page;
    }

    static MenuPage* buildSensorsPage(void* context) {
        MenuPage* page = new MenuPage(F("Sensors"), NavigationManager::instance().getCurrentPage());
        DeviceRegistry& registry = DeviceRegistry::instance();
        DynamicArray<IDevice*>& devices = registry.getDevices();
        
        // FIX: Scansiona tutti i device e aggiungi i sensori
        for (size_t i = 0; i < devices.size(); i++) {
            IDevice* d = devices[i];
            if (d->isSensor()) {
                if (d->getType() == DeviceType::SensorTemperature) {
                    page->addItem(new SensorDisplayItem(d, F("C")));
                } else if (d->getType() == DeviceType::SensorLight) {
                    page->addItem(new SensorDisplayItem(d, F("%")));
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

// Definizione di NavigationManager::handleInput DOPO SubMenuItem
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