#ifndef FLEXIBLE_MENU_H
#define FLEXIBLE_MENU_H

#include <Arduino.h>
extern "C" {
    #include "lcd.h"
}
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
    virtual void draw(uint8_t row, bool selected) = 0;
    virtual void handleInput(char key) {}
    virtual MenuItemType getType() const { return MenuItemType::GENERIC; }
};

class MenuPage : public MenuItem, public IEventListener {
private:
    const char* _title;
    DynamicArray<MenuItem*> _items;
    MenuPage* _parent;
    size_t _selected_index;
    size_t _scroll_offset;
    bool _needs_redraw;
    
    friend class NavigationManager;

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

    void draw(uint8_t row, bool selected) override {
        LCD_set_cursor(0, row);
        LCD_write_str(selected ? "> " : "  ");
        LCD_write_str(_title);
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
            if (_selected_index < static_cast<size_t>(_items.size() - 1)) {
                _selected_index++; 
                
                if (_selected_index >= _scroll_offset + 3) {
                    _scroll_offset = _selected_index - 2;
                }
                
                _needs_redraw = true; 
            }
        }
    }

    bool needsRedraw() const { return _needs_redraw; }
    void clearRedraw() { _needs_redraw = false; }
    void forceRedraw() { _needs_redraw = true; }

    void handleEvent(EventType type, IDevice* device, int value) override {
        _needs_redraw = true;
    }
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

    void setLCD() {
        // LCD già inizializzato con LCD_init()
        _initialized = true;
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
            if (key == 'B') {
                navigateBack();
            } else {
                current->handleInput(key);
            }
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
        if (!current || !_initialized) return;

        LCD_clear();
        LCD_set_cursor(0, 0);
        LCD_write_str(current->getTitle());
        
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
};

// --- Concrete Menu Items ---

class DeviceToggleItem : public MenuItem {
private:
    IDevice* _device;

public:
    DeviceToggleItem(IDevice* device) : _device(device) {}

    void draw(uint8_t row, bool selected) override {
        LCD_set_cursor(0, row);
        LCD_write_str(selected ? "> " : "  ");
        LCD_write_str(_device->getName());
        LCD_set_cursor(15, row);
        
        if (_device->getType() == DeviceType::LightSimple) {
            bool state = static_cast<SimpleLight*>(_device)->getState();
            LCD_write_str(state ? "ON " : "OFF");
        } else if (_device->getType() == DeviceType::LightDimmable) {
            int val = static_cast<DimmableLight*>(_device)->getBrightness();
            char buf[5];
            itoa(val, buf, 10);
            LCD_write_str(buf);
            LCD_write_char('%');
        }
    }

    void handleInput(char key) override {
        if (key == 'E') {
            if (_device->getType() == DeviceType::LightSimple) {
                static_cast<SimpleLight*>(_device)->toggle();
            } else if (_device->getType() == DeviceType::LightDimmable) {
                static_cast<DimmableLight*>(_device)->toggle();
            }
        }
    }
};

class ToggleActionItem : public MenuItem {
private:
    IDevice* _device;
    
public:
    ToggleActionItem(IDevice* device) : _device(device) {}
    
    void draw(uint8_t row, bool selected) override {
        LCD_set_cursor(0, row);
        LCD_write_str(selected ? "> " : "  ");
        LCD_write_str("Toggle ON/OFF");
        LCD_set_cursor(16, row);
        
        if (_device->getType() == DeviceType::LightDimmable) {
            DimmableLight* light = static_cast<DimmableLight*>(_device);
            LCD_write_str(light->getBrightness() > 0 ? "ON " : "OFF");
        }
    }
    
    void handleInput(char key) override {
        if (key == 'E') {
            if (_device->getType() == DeviceType::LightDimmable) {
                static_cast<DimmableLight*>(_device)->toggle();
            }
        }
    }
};

class BrightnessSliderItem : public MenuItem {
private:
    DimmableLight* _light;
    
public:
    BrightnessSliderItem(DimmableLight* light) : _light(light) {}
    
    void draw(uint8_t row, bool selected) override {
        LCD_set_cursor(0, row);
        LCD_write_str("Value: ");
        
        int brightness = _light->getBrightness();
        char buf[5];
        if (brightness < 10) LCD_write_char(' ');
        if (brightness < 100) LCD_write_char(' ');
        itoa(brightness, buf, 10);
        LCD_write_str(buf);
        LCD_write_char('%');
        
        // Barra grafica
        LCD_set_cursor(0, row + 1);
        LCD_write_char('[');
        int bars = map(brightness, 0, 100, 0, 18);
        for (int i = 0; i < 18; i++) {
            LCD_write_char(i < bars ? (char)0xFF : ' ');
        }
        LCD_write_char(']');
    }
    
    void handleInput(char key) override {
        int current = _light->getBrightness();
        
        if (key == 'U') {
            int newVal = min(100, current + 10);
            _light->setBrightness(newVal);
            EventSystem::instance().emit(EventType::DeviceValueChanged, _light, newVal);
        } 
        else if (key == 'D') {
            int newVal = max(0, current - 10);
            _light->setBrightness(newVal);
            EventSystem::instance().emit(EventType::DeviceValueChanged, _light, newVal);
        }
    }
};

class SensorStatsItem : public MenuItem {
private:
    TemperatureSensor* _sensor;
    uint8_t _lineOffset;
    
public:
    SensorStatsItem(TemperatureSensor* sensor, uint8_t lineOffset) 
        : _sensor(sensor), _lineOffset(lineOffset) {}
    
    void draw(uint8_t row, bool selected) override {
        SensorStats& stats = _sensor->getStats();
        LCD_set_cursor(0, row);
        
        char buf[10];
        switch(_lineOffset) {
            case 0:
                LCD_write_str("Cur: ");
                dtostrf(_sensor->getTemperature(), 0, 1, buf);
                LCD_write_str(buf);
                LCD_write_char('C');
                break;
            case 1:
                LCD_write_str("Min: ");
                dtostrf(stats.getMin(), 0, 1, buf);
                LCD_write_str(buf);
                LCD_write_char('C');
                break;
            case 2:
                LCD_write_str("Max: ");
                dtostrf(stats.getMax(), 0, 1, buf);
                LCD_write_str(buf);
                LCD_write_char('C');
                break;
            case 3:
                LCD_write_str("Avg: ");
                dtostrf(stats.getAverage(), 0, 1, buf);
                LCD_write_str(buf);
                LCD_write_char('C');
                break;
        }
    }
};

class SensorDisplayItem : public MenuItem {
private:
    IDevice* _sensor;

public:
    SensorDisplayItem(IDevice* sensor) : _sensor(sensor) {}

    void draw(uint8_t row, bool selected) override {
        LCD_set_cursor(0, row);
        LCD_write_str(selected ? "> " : "  ");
        LCD_write_str(_sensor->getName());
        LCD_set_cursor(14, row);
        
        if (_sensor->getType() == DeviceType::SensorTemperature) {
            float t = static_cast<TemperatureSensor*>(_sensor)->getTemperature();
            char buf[10];
            dtostrf(t, 0, 1, buf);
            LCD_write_str(buf);
            LCD_write_char('C');
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

    void draw(uint8_t row, bool selected) override {
        LCD_set_cursor(0, row);
        LCD_write_str(selected ? "> " : "  ");
        LCD_write_str(_label);
        LCD_write_str(" >");
    }

    void handleInput(char key) override {
        if (key == 'E') {
            _nav.navigateTo(_target);
        }
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
    
    void handleInput(char key) override {
        if (key == 'E') {
            _nav.navigateBack();
        }
    }
};

// --- Menu Builder Helper ---
class MenuBuilder {
public:
    static MenuPage* buildBrightnessPage(DimmableLight* light, MenuPage* parent) {
        MenuPage* page = new MenuPage("Set Brightness", parent);
        page->addItem(new BrightnessSliderItem(light));
        return page;
    }

    static MenuPage* buildDimmableLightPage(DimmableLight* light, MenuPage* parent) {
        MenuPage* page = new MenuPage(light->getName(), parent);
        page->addItem(new ToggleActionItem(light));
        
        MenuPage* brightnessPage = buildBrightnessPage(light, page);
        page->addItem(new SubMenuItem("Set Brightness", brightnessPage, NavigationManager::instance()));
        page->addItem(new BackMenuItem(NavigationManager::instance()));
        
        return page;
    }

    static MenuPage* buildSensorDetailPage(TemperatureSensor* sensor, MenuPage* parent) {
        MenuPage* page = new MenuPage(sensor->getName(), parent);
        page->addItem(new SensorStatsItem(sensor, 0));
        page->addItem(new SensorStatsItem(sensor, 1));
        page->addItem(new SensorStatsItem(sensor, 2));
        page->addItem(new SensorStatsItem(sensor, 3));
        page->addItem(new BackMenuItem(NavigationManager::instance()));
        return page;
    }

    static MenuPage* buildLightsPage(MenuPage* parent) {
        MenuPage* page = new MenuPage("Lights", parent);
        DeviceRegistry& registry = DeviceRegistry::instance();
        
        DynamicArray<IDevice*>& devices = registry.getDevices();
        for (size_t i = 0; i < devices.size(); i++) {
            IDevice* current_device = devices[i];
            
            if (current_device->getType() == DeviceType::LightSimple) {
                page->addItem(new DeviceToggleItem(current_device));
            }
            else if (current_device->getType() == DeviceType::LightDimmable) {
                DimmableLight* dimmable = static_cast<DimmableLight*>(current_device);
                MenuPage* lightSubMenu = buildDimmableLightPage(dimmable, page);
                page->addItem(new SubMenuItem(current_device->getName(), lightSubMenu, NavigationManager::instance()));
            }
        }
        page->addItem(new BackMenuItem(NavigationManager::instance()));
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
                MenuPage* detailPage = buildSensorDetailPage(sensor, page);
                page->addItem(new SubMenuItem(sensor->getName(), detailPage, NavigationManager::instance()));
            }
        }
        page->addItem(new BackMenuItem(NavigationManager::instance()));
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