#ifndef FLEXIBLE_MENU_H
#define FLEXIBLE_MENU_H

#include <LiquidCrystal_I2C.h>
#include "Devices.h"

// Forward declaration
class MenuPage;

enum class MenuItemType {
    GENERIC,
    SUBMENU
};

class MenuItem {
public:
    virtual ~MenuItem() {}
    virtual void draw(LiquidCrystal_I2C& lcd, uint8_t row, bool selected) = 0;
    virtual void handleInput(char key) {}
    virtual MenuItemType getType() const { return MenuItemType::GENERIC; }
};

// Corretto: eredita da IEventListener invece di IEventHandler
class MenuPage : public MenuItem, public IEventListener {
private:
    const char* _title;
    DynamicArray<MenuItem*> _items;
    MenuPage* _parent;
    size_t _selected_index;
    size_t _scroll_offset;
    bool _needs_redraw;
    
    friend class NavigationManager; // Permette accesso a _scroll_offset

public:
    MenuPage(const char* title, MenuPage* parent = nullptr) 
        : _title(title), _parent(parent), _selected_index(0), _scroll_offset(0), _needs_redraw(true) {
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
    const char* getTitle() const { return _title; }
    const char* getName() const { return _title; }
    MenuPage* getParent() const { return _parent; }
    size_t getSelectedIndex() const { return _selected_index; }

    void draw(LiquidCrystal_I2C& lcd, uint8_t row, bool selected) override {
        lcd.setCursor(0, row);
        lcd.print(selected ? "> " : "  ");
        lcd.print(_title);
    }

    virtual void handleInput(char key) {
        if (key == 'U') {
            if (_selected_index > 0) { 
                _selected_index--; 
                
                if (_selected_index < _scroll_offset) {
                    _scroll_offset = _selected_index;
                }
                
                _needs_redraw = true; 
            }
        } else if (key == 'D') {
            if (_selected_index < static_cast<size_t>(_items.size() - 1)) {  // Fix warning
                _selected_index++; 
                
                if (_selected_index >= _scroll_offset + 3) {
                    _scroll_offset = _selected_index - 2;
                }
                
                _needs_redraw = true; 
            }
        }
        // Rimosso else if per 'E' - viene gestito in main.cpp
    }

    bool needsRedraw() const {
        return _needs_redraw;
    }

    void clearRedraw() {
        _needs_redraw = false;
    }

    void forceRedraw() { _needs_redraw = true; }

    void handleEvent(EventType type, IDevice* device, int value) override {
        for (size_t i = 0; i < _items.size(); i++) {
            // Propagate event to items if needed, or just redraw
        }
        _needs_redraw = true;
    }
};

class NavigationManager {
private:
    DynamicArray<MenuPage*> _stack;
    LiquidCrystal_I2C* _lcd;

    NavigationManager() : _lcd(nullptr) {}

public:
    static NavigationManager& instance() {
        static NavigationManager inst;
        return inst;
    }

    void setLCD(LiquidCrystal_I2C& lcd) {
        _lcd = &lcd;
    }

    void initialize(MenuPage* root) {
        _stack.add(root);
        draw();
    }

    void navigateTo(MenuPage* page) {
        if (page) {
            _stack.add(page);
            page->forceRedraw();
            draw();
        }
    }

    void navigateBack() {
        if (_stack.size() > 1) {
            _stack.remove(_stack.size() - 1);
            getCurrentPage()->forceRedraw();
            draw();
        }
    }

    MenuPage* getCurrentPage() {
        if (_stack.size() > 0) return _stack[_stack.size() - 1];
        return nullptr;
    }

    void handleInput(char key) {
        MenuPage* current = getCurrentPage();
        if (current) {
            if (key == 'B') { // Back
                navigateBack();
            } else {
                current->handleInput(key);
            }
            
            // Forza sempre il redraw dopo input
            current->forceRedraw();
        }
    }

    void update() {
        MenuPage* current = getCurrentPage();
        if (current && current->needsRedraw()) {
            draw();
            current->clearRedraw();
        }
    }

    void draw() {
        MenuPage* current = getCurrentPage();
        if (!current || !_lcd) return;

        _lcd->clear();
        _lcd->setCursor(0, 0);
        _lcd->print(current->getTitle());
        
        // Draw visible items con scrolling
        size_t count = current->getItemsCount();
        size_t max_lines = 3; // 4 lines total, 1 for title
        size_t scroll_offset = current->_scroll_offset;
        
        for (size_t i = 0; i < min(count - scroll_offset, max_lines); i++) {
            size_t itemIdx = i + scroll_offset;
            MenuItem* item = current->getItem(itemIdx);
            item->draw(*_lcd, i + 1, itemIdx == current->getSelectedIndex());
        }
        
        // Mostra indicatore scroll se necessario
        if (scroll_offset > 0) {
            _lcd->setCursor(19, 1);
            _lcd->print((char)0x5E); // Freccia su '^'
        }
        if (scroll_offset + max_lines < count) {
            _lcd->setCursor(19, 3);
            _lcd->print((char)0x76); // Freccia giù 'v'
        }
    }
};

// --- Concrete Menu Items ---

class DeviceToggleItem : public MenuItem {
private:
    IDevice* _device;

public:
    DeviceToggleItem(IDevice* device) : _device(device) {}

    void draw(LiquidCrystal_I2C& lcd, uint8_t row, bool selected) override {
        lcd.setCursor(0, row);
        lcd.print(selected ? "> " : "  ");
        lcd.print(_device->getName());
        lcd.setCursor(15, row);
        
        if (_device->getType() == DeviceType::LightSimple) {
             bool state = static_cast<SimpleLight*>(_device)->getState();
             lcd.print(state ? "ON " : "OFF");
        } else if (_device->getType() == DeviceType::LightDimmable) {
             int val = static_cast<DimmableLight*>(_device)->getBrightness();
             lcd.print(val);
             lcd.print("%");
        }
    }

    void handleInput(char key) override {
        if (key == 'E') {
            if (_device->getType() == DeviceType::LightSimple) {
                SimpleLight* light = static_cast<SimpleLight*>(_device);
                light->toggle();
                // OTTIMIZZAZIONE: Debug rimosso (consuma stack)
            } else if (_device->getType() == DeviceType::LightDimmable) {
                DimmableLight* light = static_cast<DimmableLight*>(_device);
                light->toggle();
                // OTTIMIZZAZIONE: Debug rimosso
            }
        }
    }
};

// NUOVO: Item per toggle all'interno di un sottomenu
class ToggleActionItem : public MenuItem {
private:
    IDevice* _device;
    
public:
    ToggleActionItem(IDevice* device) : _device(device) {}
    
    void draw(LiquidCrystal_I2C& lcd, uint8_t row, bool selected) override {
        lcd.setCursor(0, row);
        lcd.print(selected ? "> " : "  ");
        lcd.print(F("Toggle ON/OFF"));
        lcd.setCursor(16, row);
        
        if (_device->getType() == DeviceType::LightDimmable) {
            DimmableLight* light = static_cast<DimmableLight*>(_device);
            lcd.print(light->getBrightness() > 0 ? "ON " : "OFF");
        }
    }
    
    void handleInput(char key) override {
        if (key == 'E') {
            if (_device->getType() == DeviceType::LightDimmable) {
                DimmableLight* light = static_cast<DimmableLight*>(_device);
                light->toggle();
                // OTTIMIZZAZIONE: Debug rimosso
            }
        }
    }
};

// NUOVO: Item per regolare brightness con UP/DOWN (ora interattivo nella sua pagina)
class BrightnessSliderItem : public MenuItem {
private:
    DimmableLight* _light;
    
public:
    BrightnessSliderItem(DimmableLight* light) : _light(light) {}
    
    void draw(LiquidCrystal_I2C& lcd, uint8_t row, bool selected) override {
        lcd.setCursor(0, row);
        lcd.print(F("Value: "));
        
        int brightness = _light->getBrightness();
        if (brightness < 10) lcd.print(" ");
        if (brightness < 100) lcd.print(" ");
        lcd.print(brightness);
        lcd.print(F("%"));
        
        // Barra grafica
        lcd.setCursor(0, row + 1);
        lcd.print(F("["));
        int bars = map(brightness, 0, 100, 0, 18);
        for (int i = 0; i < 18; i++) {
            lcd.print(i < bars ? (char)0xFF : ' ');
        }
        lcd.print(F("]"));
    }
    
    void handleInput(char key) override {
        int current = _light->getBrightness();
        
        if (key == 'U') {
            int newVal = min(100, current + 10);
            _light->setBrightness(newVal);
            // OTTIMIZZAZIONE: Debug rimosso
            EventSystem::instance().emit(EventType::DeviceValueChanged, _light, newVal);
        } 
        else if (key == 'D') {
            int newVal = max(0, current - 10);
            _light->setBrightness(newVal);
            // OTTIMIZZAZIONE: Debug rimosso
            EventSystem::instance().emit(EventType::DeviceValueChanged, _light, newVal);
        }
    }
};

// NUOVO: Item per mostrare statistiche sensore
class SensorStatsItem : public MenuItem {
private:
    TemperatureSensor* _sensor;
    uint8_t _lineOffset;
    
public:
    SensorStatsItem(TemperatureSensor* sensor, uint8_t lineOffset) 
        : _sensor(sensor), _lineOffset(lineOffset) {}
    
    void draw(LiquidCrystal_I2C& lcd, uint8_t row, bool selected) override {
        SensorStats& stats = _sensor->getStats();
        
        // Non mostrare cursore per statistiche
        lcd.setCursor(0, row);
        
        switch(_lineOffset) {
            case 0:
                lcd.print(F("Cur: "));
                lcd.print(_sensor->getTemperature(), 1);
                lcd.print(F("C"));
                break;
            case 1:
                lcd.print(F("Min: "));
                lcd.print(stats.getMin(), 1);
                lcd.print(F("C"));
                break;
            case 2:
                lcd.print(F("Max: "));
                lcd.print(stats.getMax(), 1);
                lcd.print(F("C"));
                break;
            case 3:
                lcd.print(F("Avg: "));
                lcd.print(stats.getAverage(), 1);
                lcd.print(F("C"));
                break;
        }
    }
};

class SensorDisplayItem : public MenuItem {
private:
    IDevice* _sensor;

public:
    SensorDisplayItem(IDevice* sensor) : _sensor(sensor) {}

    void draw(LiquidCrystal_I2C& lcd, uint8_t row, bool selected) override {
        lcd.setCursor(0, row);
        lcd.print(selected ? "> " : "  ");
        lcd.print(_sensor->getName());
        lcd.setCursor(14, row);
        
        if (_sensor->getType() == DeviceType::SensorTemperature) {
            float t = static_cast<TemperatureSensor*>(_sensor)->getTemperature();
            lcd.print(t, 1);
            lcd.print("C");
        }
    }
};

class SubMenuItem : public MenuItem {
private:
    const char* _label;
    MenuPage* _target;
    NavigationManager& _nav;

public:
    SubMenuItem(const char* label, MenuPage* target, NavigationManager& nav) 
        : _label(label), _target(target), _nav(nav) {}

    MenuItemType getType() const override { return MenuItemType::SUBMENU; }
    MenuPage* getTarget() const { return _target; }

    void draw(LiquidCrystal_I2C& lcd, uint8_t row, bool selected) override {
        lcd.setCursor(0, row);
        lcd.print(selected ? "> " : "  ");
        lcd.print(_label);
        lcd.print(" >");
    }

    void handleInput(char key) override {
        if (key == 'E') {
            _nav.navigateTo(_target);
        }
    }
};

// NUOVO: Item per tornare indietro (alternativa a pressione fisica)
class BackMenuItem : public MenuItem {
private:
    NavigationManager& _nav;
    
public:
    BackMenuItem(NavigationManager& nav) : _nav(nav) {}
    
    void draw(LiquidCrystal_I2C& lcd, uint8_t row, bool selected) override {
        lcd.setCursor(0, row);
        lcd.print(selected ? "> " : "  ");
        lcd.print(F("<< Back"));
    }
    
    void handleInput(char key) override {
        if (key == 'E') {
            _nav.navigateBack();
        }
    }
};

// --- Menu Builder Helper ---
class MenuBuilder {
public:
    // NUOVO: Costruisce pagina slider brightness
    static MenuPage* buildBrightnessPage(DimmableLight* light, MenuPage* parent) {
        MenuPage* page = new MenuPage("Set Brightness", parent);
        page->addItem(new BrightnessSliderItem(light));
        page->addItem(new BackMenuItem(NavigationManager::instance()));  // NUOVO
        return page;
    }

    // MODIFICATO: Sottomenu luce dimmabile ora ha "Set Brightness" come sottomenu
    static MenuPage* buildDimmableLightPage(DimmableLight* light, MenuPage* parent) {
        MenuPage* page = new MenuPage(light->getName(), parent);
        page->addItem(new ToggleActionItem(light));
        
        MenuPage* brightnessPage = buildBrightnessPage(light, page);
        page->addItem(new SubMenuItem("Set Brightness", brightnessPage, NavigationManager::instance()));
        page->addItem(new BackMenuItem(NavigationManager::instance()));  // NUOVO
        
        return page;
    }

    // NUOVO: Costruisce pagina dettaglio sensore con statistiche
    static MenuPage* buildSensorDetailPage(TemperatureSensor* sensor, MenuPage* parent) {
        MenuPage* page = new MenuPage(sensor->getName(), parent);
        page->addItem(new SensorStatsItem(sensor, 0));
        page->addItem(new SensorStatsItem(sensor, 1));
        page->addItem(new SensorStatsItem(sensor, 2));
        page->addItem(new SensorStatsItem(sensor, 3));
        page->addItem(new BackMenuItem(NavigationManager::instance()));  // NUOVO
        return page;
    }

    static MenuPage* buildLightsPage(MenuPage* parent) {
        MenuPage* page = new MenuPage("Lights", parent);
        DeviceRegistry& registry = DeviceRegistry::instance();
        
        DynamicArray<IDevice*>& devices = registry.getDevices();
        for (size_t i = 0; i < devices.size(); i++) {
            IDevice* current_device = devices[i];
            
            if (current_device->getType() == DeviceType::LightSimple) {
                // Luce semplice: toggle diretto
                page->addItem(new DeviceToggleItem(current_device));
            }
            else if (current_device->getType() == DeviceType::LightDimmable) {
                // Luce dimmabile: crea sottomenu
                DimmableLight* dimmable = static_cast<DimmableLight*>(current_device);
                MenuPage* lightSubMenu = buildDimmableLightPage(dimmable, page);
                page->addItem(new SubMenuItem(current_device->getName(), lightSubMenu, NavigationManager::instance()));
            }
        }
        page->addItem(new BackMenuItem(NavigationManager::instance()));  // NUOVO
        return page;
    }

    static MenuPage* buildSensorsPage(MenuPage* parent) {
        MenuPage* page = new MenuPage("Sensors", parent);
        DeviceRegistry& registry = DeviceRegistry::instance();
        
        DynamicArray<IDevice*>& devices = registry.getDevices();
        for (size_t i = 0; i < devices.size(); i++) {
            IDevice* current_device = devices[i];
            if (current_device->isSensor() && current_device->getType() == DeviceType::SensorTemperature) {
                TemperatureSensor* sensor = static_cast<TemperatureSensor*>(current_device);
                
                // Crea sottomenu per ogni sensore
                MenuPage* detailPage = buildSensorDetailPage(sensor, page);
                page->addItem(new SubMenuItem(sensor->getName(), detailPage, NavigationManager::instance()));
            }
        }
        page->addItem(new BackMenuItem(NavigationManager::instance()));  // NUOVO
        return page;
    }

    static MenuPage* buildMainMenu() {
        MenuPage* root = new MenuPage("Main Menu");
        root->addItem(new SubMenuItem("Lights", buildLightsPage(root), NavigationManager::instance()));
        root->addItem(new SubMenuItem("Sensors", buildSensorsPage(root), NavigationManager::instance()));
        return root;
    }
};

#endif