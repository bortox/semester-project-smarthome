/**
 * @file FlexibleMenu.h
 * @brief Dynamic menu system with JIT page allocation and scene integration
 * @ingroup Core
 * 
 * Implements a memory-efficient menu system for LCD displays using Just-In-Time page creation.
 * Pages are allocated on heap when entering and freed when exiting to minimize RAM usage.
 * Supports device control, sensor monitoring, and scene management.
 */
#ifndef FLEXIBLE_MENU_H
#define FLEXIBLE_MENU_H

#include <Arduino.h>
#include <avr/pgmspace.h>
extern "C" {
    #include "lcd.h"
}
#include "Devices.h"
#include "Scenes.h"      // FIX: Include for scene types

class MenuPage;
class NavigationManager;
class SubMenuItem;

/**
 * @brief Base menu item type discriminator
 * @ingroup MenuCore
 */
enum class MenuItemType {
    GENERIC,  ///< Standard menu item
    SUBMENU   ///< Item that opens a submenu
};

/**
 * @brief Abstract base class for all menu items
 * @ingroup MenuCore
 * 
 * Provides interface for rendering and input handling. Items can optionally
 * relate to specific devices for event-driven updates.
 */
class MenuItem {
public:
    virtual ~MenuItem() {}
    
    /**
     * @brief Renders the item on LCD
     * @param row LCD row (0-3)
     * @param selected True if item is currently selected
     */
    virtual void draw(uint8_t row, bool selected) = 0;
    
    /**
     * @brief Handles user input
     * @param event Input event (UP, DOWN, ENTER, BACK)
     * @return True if input was handled, false otherwise
     */
    virtual bool handleInput(InputEvent event) { return false; }
    
    /**
     * @brief Returns item type for polymorphic dispatch
     * @return MenuItemType enum value
     */
    virtual MenuItemType getType() const { return MenuItemType::GENERIC; }
    
    /**
     * @brief Checks if item is related to a specific device
     * @param device Device pointer to check
     * @return True if item should update when device changes
     */
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

/**
 * @brief Menu page container with event-driven updates
 * @ingroup MenuCore
 * 
 * Manages a collection of menu items with scrolling support. Listens to device
 * events and triggers redraws when related devices change. Supports JIT lifecycle
 * when used with NavigationManager stack.
 */
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
    /**
     * @brief Constructs a menu page
     * @param title Flash string title displayed at top
     * @param parent Parent page (optional, for static hierarchies)
     */
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

    /**
     * @brief Adds item to page
     * @param item Heap-allocated item (page takes ownership)
     */
    void addItem(MenuItem* item) { _items.add(item); }
    
    /**
     * @brief Gets item by index
     * @param idx Item index
     * @return Pointer to item or nullptr if out of bounds
     */
    MenuItem* getItem(size_t idx) { return _items[idx]; }
    
    /**
     * @brief Gets total number of items
     * @return Item count
     */
    size_t getItemsCount() const { return _items.size(); }
    
    /**
     * @brief Gets parent page
     * @return Parent pointer or nullptr for root
     */
    MenuPage* getParent() const { return _parent; }
    
    /**
     * @brief Gets currently selected item index
     * @return Selected index
     */
    size_t getSelectedIndex() const { return _selected_index; }

    void draw(uint8_t row, bool selected) override {
        LCD_set_cursor(0, row);
        LCD_write_str(selected ? "> " : "  ");
        printLabel(_title);
    }

    /**
     * @brief Handles navigation input and delegates to items
     * @param event Input event
     * @return True if input was handled
     */
    bool handleInput(InputEvent event) override;
    
    /**
     * @brief Handles device events for related items
     * @param type Event type
     * @param device Device that triggered event
     * @param value Associated value
     */
    void handleEvent(EventType type, IDevice* device, int value) override;
    
    /**
     * @brief Checks if page needs redrawing
     * @return True if redraw required
     */
    bool needsRedraw() const { return _needs_redraw; }
    
    /**
     * @brief Clears redraw flag after rendering
     */
    void clearRedraw() { _needs_redraw = false; }
    
    /**
     * @brief Forces full page redraw
     */
    void forceRedraw() { _needs_redraw = true; }
};

/**
 * @brief Singleton menu navigation manager with JIT page allocation
 * @ingroup MenuCore
 * 
 * Manages a stack of MenuPage instances. Implements Just-In-Time strategy:
 * - Pages are created on heap when entering (pushPage)
 * - Pages are deleted when exiting (navigateBack)
 * This minimizes RAM usage for complex menu hierarchies.
 */
class NavigationManager {
private:
    DynamicArray<MenuPage*> _stack; ///< Page navigation stack
    bool _initialized;              ///< LCD initialization flag

    NavigationManager() : _initialized(false) {}

public:
    /**
     * @brief Gets singleton instance
     * @return Reference to navigation manager
     */
    static NavigationManager& instance() {
        static NavigationManager inst;
        return inst;
    }

    /**
     * @brief Marks LCD as initialized
     */
    void setLCD() { _initialized = true; }

    /**
     * @brief Initializes navigation with root page
     * @param root Pre-allocated root page (not JIT)
     */
    void initialize(MenuPage* root) {
        _stack.add(root);
        draw();
    }

    /**
     * @brief Pushes JIT-allocated page onto stack
     * @param page Heap-allocated page (manager takes ownership)
     */
    void pushPage(MenuPage* page) {
        if (page) {
            _stack.add(page);
            page->forceRedraw();
            draw();
        }
    }

    /**
     * @brief Navigates back and frees current page (JIT cleanup)
     * 
     * Pops current page from stack and deletes it to free RAM.
     * Root page is never deleted.
     */
    void navigateBack() {
        if (_stack.size() > 1) {
            MenuPage* current = _stack[_stack.size() - 1];
            _stack.remove(_stack.size() - 1);
            delete current; // JIT: Free RAM
            
            getCurrentPage()->forceRedraw();
            draw();
        }
    }

    /**
     * @brief Gets current page from top of stack
     * @return Current page or nullptr if stack empty
     */
    MenuPage* getCurrentPage() {
        if (_stack.size() > 0) return _stack[_stack.size() - 1];
        return nullptr;
    }

    /**
     * @brief Delegates input to current page
     * @param event Input event
     */
    void handleInput(InputEvent event);

    /**
     * @brief Updates display if current page needs redraw
     */
    void update() {
        MenuPage* current = getCurrentPage();
        if (current && current->needsRedraw()) {
            draw();
            current->clearRedraw();
        }
    }

    /**
     * @brief Incrementally updates cursor position
     * @param oldIndex Previous selection index
     * @param newIndex New selection index
     */
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

    /**
     * @brief Renders full page with scrolling support
     */
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
inline bool MenuPage::handleInput(InputEvent event) {
    size_t oldIndex = _selected_index;

    // FIX: First try to pass input to the current item
    if (_selected_index < _items.size()) {
        bool handled = _items[_selected_index]->handleInput(event);
        if (handled) {
            _needs_redraw = true;
            return true; // FIX: Propagate that it was handled
        }
    }

    // If item didn't handle input, perform normal navigation
    if (event == InputEvent::UP) {
        if (_selected_index > 0) { 
            _selected_index--; 
            
            if (_selected_index < _scroll_offset) {
                _scroll_offset = _selected_index;
                _needs_redraw = true;
            } else {
                NavigationManager::instance().drawIncrementalCursor(oldIndex, _selected_index);
                return true; // FIX: Handled by navigation
            }
        }
        return true; // FIX: Handled (even if no movement)
    } else if (event == InputEvent::DOWN) {
        if (_selected_index < static_cast<size_t>(_items.size() - 1)) {
            _selected_index++; 
            
            if (_selected_index >= _scroll_offset + 3) {
                _scroll_offset = _selected_index - 2;
                _needs_redraw = true;
            } else {
                NavigationManager::instance().drawIncrementalCursor(oldIndex, _selected_index);
                return true; // FIX: Handled by navigation
            }
        }
        return true; // FIX: Handled (anche se non c'è movimento)
    }
    
    return false; // Other keys not handled
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

/**
 * @brief Toggle control for light devices
 * @ingroup MenuItems
 */
class DeviceToggleItem : public MenuItem {
private:
    IDevice* _device;
public:
    /**
     * @brief Constructs toggle item for device
     * @param device Light device to control
     */
    DeviceToggleItem(IDevice* device) : _device(device) {}
    
    bool relatesTo(IDevice* dev) override { return _device == dev; }
    
    /**
     * @brief Draws device name and ON/OFF state
     * @param row LCD row
     * @param selected Selection state
     */
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
    /**
     * @brief Toggles device on 'E' key
     * @param event Input event
     * @return True if device was toggled
     */
    bool handleInput(InputEvent event) override {
        if (event == InputEvent::ENTER && _device->isLight()) {
            static_cast<SimpleLight*>(_device)->toggle();
            return true;
        }
        return false;
    }
};

/**
 * @brief Template-based value slider with pixel-perfect progress bar
 * @ingroup MenuItems
 * @tparam DeviceType Device class with getter/setter methods
 * 
 * Displays a two-row slider:
 * - Row 1: Label and numeric value
 * - Row 2: 100-pixel wide progress bar (20 chars × 5 pixels each)
 */
template<typename DeviceType>
class ValueSliderItem : public MenuItem {
private:
    DeviceType* _device;
    const __FlashStringHelper* _label;
    uint8_t (DeviceType::*_getter)() const;
    void (DeviceType::*_setter)(uint8_t);
    uint8_t _min, _max, _step;
    
    static bool _customCharsLoaded;
    
    void loadCustomChars() {
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

public:
    /**
     * @brief Constructs slider with member function pointers
     * @param device Target device instance
     * @param label Flash string label
     * @param getter Member function returning current value
     * @param setter Member function accepting new value
     * @param minVal Minimum allowed value
     * @param maxVal Maximum allowed value
     * @param step Increment/decrement step size
     */
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

    /**
     * @brief Draws label, value, and progress bar
     * @param row LCD row for label (bar drawn on row+1)
     * @param selected Selection state
     */
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

        // Row 1: Full-width progress bar (20 chars = 100 pixels)
        LCD_set_cursor(0, row + 1);
        
        // Map value to pixels (0-100 pixels across 20 chars)
        uint16_t totalPixels = map(val, _min, _max, 0, 100);
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

    /**
     * @brief Handles up/down input to adjust value
     * @param event Input event (UP=increment, DOWN=decrement)
     * @return True if value was changed
     */
    bool handleInput(InputEvent event) override {
        uint8_t current = (_device->*_getter)();
        
        if (event == InputEvent::UP) {
            // Safely increment with clamping
            uint8_t newVal;
            if (current > _max - _step) {
                newVal = _max;
            } else {
                newVal = current + _step;
            }
            (_device->*_setter)(newVal);
            return true;
            
        } else if (event == InputEvent::DOWN) {
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

// Static member initialization
template<typename DeviceType>
bool ValueSliderItem<DeviceType>::_customCharsLoaded = false;

/**
 * @brief Helper factory for slider creation with type deduction
 * @ingroup MenuItems
 * @tparam DeviceType Automatically deduced from device parameter
 * @param device Device instance
 * @param label Display label
 * @param getter Value getter method
 * @param setter Value setter method
 * @param minVal Minimum value
 * @param maxVal Maximum value
 * @param step Step size
 * @return Heap-allocated slider item
 */
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

/**
 * @brief Template-based live value display with automatic updates
 * @ingroup MenuItems
 * @tparam T Object type containing the value getter method
 * 
 * Displays real-time values from sensors or statistics objects.
 * Automatically updates when related device emits events.
 */
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
    /**
     * @brief Constructs live item for statistics display
     * @param label Display label
     * @param object Object containing getter method
     * @param getter Member function returning int16_t value
     * @param unit Unit string (e.g., "C", "%")
     * @param isTemp True for temperature formatting (XX.X°)
     */
    LiveItem(const __FlashStringHelper* label, T* object, int16_t (T::*getter)() const,
             const __FlashStringHelper* unit, bool isTemp = false)
        : _label(label), _object(object), _getter(getter), _unit(unit), 
          _isTemperature(isTemp), _device(nullptr) {}

    /**
     * @brief Constructs live item for sensor display
     * @param device Device for event correlation
     * @param object Object containing getter method
     * @param getter Member function returning int16_t value
     * @param unit Unit string
     * @param isTemp True for temperature formatting
     */
    LiveItem(IDevice* device, T* object, int16_t (T::*getter)() const,
             const __FlashStringHelper* unit, bool isTemp = false)
        : _label(nullptr), _object(object), _getter(getter), _unit(unit),
          _isTemperature(isTemp), _device(device) {}

    bool relatesTo(IDevice* dev) override { return _device == dev; }

    /**
     * @brief Draws sensor name and value with unit
     * @param row LCD row
     * @param selected Selection state
     */
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

    bool handleInput(InputEvent event) override { return false; }
};

// Helper factory for live item creation with type deduction
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

/**
 * @brief Overload for device-based live item creation
 * @ingroup MenuItems
 */
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

/**
 * @brief Specialized live display for PIR motion sensors
 * @ingroup MenuItems
 */
class LivePIRItem : public MenuItem {
private:
    PIRSensorDevice* _sensor;

public:
    /**
     * @brief Constructs PIR display item
     * @param sensor PIR sensor device
     */
    LivePIRItem(PIRSensorDevice* sensor) : _sensor(sensor) {}

    bool relatesTo(IDevice* dev) override { return _sensor == dev; }

    /**
     * @brief Draws sensor name and Yes/No motion status
     * @param row LCD row
     * @param selected Selection state
     */
    void draw(uint8_t row, bool selected) override {
        LCD_set_cursor(0, row);
        LCD_write_str(selected ? "> " : "  ");
        LCD_write_str(_sensor->name);
        LCD_write_str(": ");
        LCD_write_str(_sensor->isMotionDetected() ? "Yes" : "No");
    }

    bool handleInput(InputEvent event) override { return false; }
};

/**
 * @brief Light sensor calibration item
 * @ingroup MenuItems
 * 
 * Allows user to set min (dark) or max (bright) calibration points
 * for photoresistor sensors.
 */
class LightCalibrationItem : public MenuItem {
private:
    const __FlashStringHelper* _label;
    PhotoresistorSensor* _sensor;
    bool _isDark;  ///< True = calibrate dark limit, False = calibrate bright limit

public:
    /**
     * @brief Constructs calibration item
     * @param label Display label
     * @param sensor Light sensor to calibrate
     * @param isDark True for dark calibration, false for bright
     */
    LightCalibrationItem(const __FlashStringHelper* label, PhotoresistorSensor* sensor, bool isDark)
        : _label(label), _sensor(sensor), _isDark(isDark) {}

    /**
     * @brief Draws calibration label
     * @param row LCD row
     * @param selected Selection state
     */
    void draw(uint8_t row, bool selected) override {
        LCD_set_cursor(0, row);
        LCD_write_str(selected ? "> " : "  ");
        printLabel(_label);
    }

    /**
     * @brief Executes calibration on 'E' key
     * @param event Input event
     * @return True if calibration was performed
     */
    bool handleInput(InputEvent event) override {
        if (event == InputEvent::ENTER) {
            if (_isDark) {
                _sensor->calibrateCurrentAsMin();
            } else {
                _sensor->calibrateCurrentAsMax();
            }
            // Force immediate update
            EventSystem::instance().emit(EventType::SensorUpdated, _sensor, _sensor->getValue());
            return true;
        }
        return false;
    }
};

/**
 * @brief Action item for scrollable lists
 * @ingroup MenuItems
 * 
 * Executes a function and automatically navigates back, useful for
 * mode selection or preset lists.
 */
class ActionItem : public MenuItem {
private:
    IDevice* _device;
    const char* _label;
    void (*_action)(IDevice*, int);
    int _param;
    NavigationManager& _nav;

public:
    /**
     * @brief Constructs action item
     * @param label Display label
     * @param device Target device
     * @param action Function pointer to execute
     * @param param Integer parameter for action
     * @param nav Navigation manager reference
     */
    ActionItem(const char* label, IDevice* device, void (*action)(IDevice*, int), int param, NavigationManager& nav)
        : _device(device), _label(label), _action(action), _param(param), _nav(nav) {}

    /**
     * @brief Draws action label
     * @param row LCD row
     * @param selected Selection state
     */
    void draw(uint8_t row, bool selected) override {
        LCD_set_cursor(0, row);
        LCD_write_str(selected ? "> " : "  ");
        LCD_write_str(_label);
    }

    /**
     * @brief Executes action and navigates back on 'E' key
     * @param event Input event
     * @return True if action was executed
     */
    bool handleInput(InputEvent event) override {
        if (event == InputEvent::ENTER) {
            _action(_device, _param);
            _nav.navigateBack();
            return true;
        }
        return false;
    }
};

/**
 * @brief Submenu item with JIT page creation
 * @ingroup MenuItems
 * 
 * Uses PageBuilder function pointer to create pages on-demand.
 * Pages are allocated when entering and freed when exiting to save RAM.
 */
class SubMenuItem : public MenuItem {
private:
    const __FlashStringHelper* _label;
    PageBuilder _builder;     ///< Function pointer for JIT page creation
    void* _context;           ///< Context data passed to builder
    NavigationManager& _nav;

public:
    /**
     * @brief Constructs JIT submenu item
     * @param label Display label
     * @param builder Function that creates page on heap
     * @param context Optional context data for builder
     * @param nav Navigation manager reference
     */
    SubMenuItem(const __FlashStringHelper* label, PageBuilder builder, void* context, NavigationManager& nav) 
        : _label(label), _builder(builder), _context(context), _nav(nav) {}

    MenuItemType getType() const override { return MenuItemType::SUBMENU; }
    
    /**
     * @brief Creates page using builder function
     * @return Heap-allocated page
     */
    MenuPage* createPage() const { return _builder(_context); }

    /**
     * @brief Draws label with '>' indicator
     * @param row LCD row
     * @param selected Selection state
     */
    void draw(uint8_t row, bool selected) override {
        LCD_set_cursor(0, row);
        LCD_write_str(selected ? "> " : "  ");
        printLabel(_label);
        LCD_write_str(" >");
    }

    /**
     * @brief Creates and pushes page on 'E' key
     * @param event Input event
     * @return True if submenu was entered
     */
    bool handleInput(InputEvent event) override {
        if (event == InputEvent::ENTER) {
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
    /**
     * @brief Constructs back item
     * @param nav Navigation manager reference
     */
    BackMenuItem(NavigationManager& nav) : _nav(nav) {}
    
    /**
     * @brief Draws "<< Back" label
     * @param row LCD row
     * @param selected Selection state
     */
    void draw(uint8_t row, bool selected) override {
        LCD_set_cursor(0, row);
        LCD_write_str(selected ? "> " : "  ");
        LCD_write_str("<< Back");
    }
    
    /**
     * @brief Navigates back on 'E' key
     * @param event Input event
     * @return True if navigation occurred
     */
    bool handleInput(InputEvent event) override {
        if (event == InputEvent::ENTER) {
            _nav.navigateBack();
            return true;
        }
        return false;
    }
};

/**
 * @brief Scene activation/deactivation control
 * @ingroup MenuItems
 */
class SceneToggleItem : public MenuItem {
private:
    IScene* _scene;

public:
    /**
     * @brief Constructs scene toggle item
     * @param scene Scene to control
     */
    SceneToggleItem(IScene* scene) : _scene(scene) {}

    /**
     * @brief Draws scene name and ON/OFF status
     * @param row LCD row
     * @param selected Selection state
     */
    void draw(uint8_t row, bool selected) override {
        LCD_set_cursor(0, row);
        LCD_write_str(selected ? "> " : "  ");
        LCD_write_str(_scene->getName());
        LCD_set_cursor(15, row);
        LCD_write_str(_scene->isActive() ? "ON " : "OFF");
    }

    /**
     * @brief Toggles scene activation on 'E' key
     * @param event Input event
     * @return True if scene was toggled
     */
    bool handleInput(InputEvent event) override {
        if (event == InputEvent::ENTER) {
            if (_scene->isActive()) {
                SceneManager::instance().removeScene(_scene);
            } else {
                SceneManager::instance().addScene(_scene);
            }
            return true;
        }
        return false;
    }
};

// --- BUILDERS (Refactored senza wrapper statiche) ---

/**
 * @brief Factory class for building menu page hierarchies
 * @ingroup MenuFactory
 * 
 * Provides static builder functions that create menu pages on-demand.
 * Used with SubMenuItem for JIT page allocation strategy.
 */
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
    /**
     * @brief Builds RGB red channel slider page
     * @param context RGBLight* device
     * @return Heap-allocated page
     */
    static MenuPage* buildRedPage(void* context) {
        RGBLight* light = static_cast<RGBLight*>(context);
        MenuPage* page = new MenuPage(F("Red Channel"), nullptr);
        if (!page) return nullptr;
        
        page->addItem(makeSlider(light, F("Red"), &RGBLight::getRed, &RGBLight::setRed, 0, 255, 3));
        page->addItem(new BackMenuItem(NavigationManager::instance()));
        return page;
    }

    /**
     * @brief Builds RGB green channel slider page
     * @param context RGBLight* device
     * @return Heap-allocated page
     */
    static MenuPage* buildGreenPage(void* context) {
        RGBLight* light = static_cast<RGBLight*>(context);
        MenuPage* page = new MenuPage(F("Green Channel"), nullptr);
        if (!page) return nullptr;
        
        page->addItem(makeSlider(light, F("Green"), &RGBLight::getGreen, &RGBLight::setGreen, 0, 255, 3));
        page->addItem(new BackMenuItem(NavigationManager::instance()));
        return page;
    }

    /**
     * @brief Builds RGB blue channel slider page
     * @param context RGBLight* device
     * @return Heap-allocated page
     */
    static MenuPage* buildBluePage(void* context) {
        RGBLight* light = static_cast<RGBLight*>(context);
        MenuPage* page = new MenuPage(F("Blue Channel"), nullptr);
        if (!page) return nullptr;
        
        page->addItem(makeSlider(light, F("Blue"), &RGBLight::getBlue, &RGBLight::setBlue, 0, 255, 3));
        page->addItem(new BackMenuItem(NavigationManager::instance()));
        return page;
    }

    /**
     * @brief Builds brightness slider page
     * @param context DimmableLight* device
     * @return Heap-allocated page
     */
    static MenuPage* buildBrightnessPage(void* context) {
        DimmableLight* light = static_cast<DimmableLight*>(context);
        MenuPage* page = new MenuPage(F("Brightness"), nullptr);
        if (!page) return nullptr;
        
        page->addItem(makeSlider(light, F("Level"), &DimmableLight::getBrightness, &DimmableLight::setBrightness, 0, 100, 15));
        return page;
    }

    /**
     * @brief Builds custom RGB color page with channel submenu
     * @param context RGBLight* device
     * @return Heap-allocated page
     */
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

    /**
     * @brief Builds RGB color preset selection page
     * @param context RGBLight* device
     * @return Heap-allocated page with preset options
     */
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

    /**
     * @brief Builds RGB light control page
     * @param context RGBLight* device
     * @return Heap-allocated page with toggle, brightness, presets
     */
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

    /**
     * @brief Builds dimmable light control page
     * @param context DimmableLight* device
     * @return Heap-allocated page with toggle and brightness
     */
    static MenuPage* buildDimmableLightPage(void* context) {
        DimmableLight* light = static_cast<DimmableLight*>(context);
        MenuPage* page = new MenuPage(F("Dimmable Light"), NavigationManager::instance().getCurrentPage());
        page->addItem(new DeviceToggleItem(light));
        page->addItem(new SubMenuItem(F("Set Brightness"), buildBrightnessPage, light, NavigationManager::instance()));
        page->addItem(new BackMenuItem(NavigationManager::instance()));
        return page;
    }

    /**
     * @brief Builds outside light mode selection page
     * @param context OutsideLight* device
     * @return Heap-allocated page with mode options
     */
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

    /**
     * @brief Builds outside light control page
     * @param context OutsideLight* device
     * @return Heap-allocated page
     */
    static MenuPage* buildOutsideLightPage(void* context) {
        OutsideLight* light = (OutsideLight*)context;
        MenuPage* page = new MenuPage(F("Outside Light"), NavigationManager::instance().getCurrentPage());
        page->addItem(new SubMenuItem(F("Set Mode"), buildOutsideModesPage, light, NavigationManager::instance()));
        page->addItem(new BackMenuItem(NavigationManager::instance()));
        return page;
    }

    /**
     * @brief Builds lights overview page
     * @param context Unused (nullptr)
     * @return Heap-allocated page listing all lights from registry
     */
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

    /**
     * @brief Builds sensor statistics page
     * @param context IDevice* sensor
     * @return Heap-allocated page with live value and min/max/avg stats
     */
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

    /**
     * @brief Builds light sensor settings page
     * @param context PhotoresistorSensor* device
     * @return Heap-allocated page with stats and calibration
     */
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

    /**
     * @brief Builds sensors overview page
     * @param context Unused (nullptr)
     * @return Heap-allocated page listing all sensors from registry
     */
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

    /**
     * @brief Builds scenes control page
     * @param context Unused (nullptr)
     * @return Heap-allocated page with scene toggle items
     */
    static MenuPage* buildScenesPage(void* context) {
        MenuPage* page = new MenuPage(F("Scenes"), NavigationManager::instance().getCurrentPage());
        
        // Aggiungi scene disponibili (create staticamente in main)
        extern NightModeScene nightMode;
        extern PartyScene partyMode;
        extern AlarmScene alarmMode;
        
        page->addItem(new SceneToggleItem(&nightMode));
        page->addItem(new SceneToggleItem(&partyMode));
        page->addItem(new SceneToggleItem(&alarmMode));
        
        page->addItem(new BackMenuItem(NavigationManager::instance()));
        return page;
    }

    /**
     * @brief Builds root menu page
     * @return Heap-allocated main menu (not JIT, never freed)
     */
    static MenuPage* buildMainMenu() {
        MenuPage* root = new MenuPage(F("Main Menu"));
        root->addItem(new SubMenuItem(F("Lights"), buildLightsPage, nullptr, NavigationManager::instance()));
        root->addItem(new SubMenuItem(F("Sensors"), buildSensorsPage, nullptr, NavigationManager::instance()));
        root->addItem(new SubMenuItem(F("Scenes"), buildScenesPage, nullptr, NavigationManager::instance()));
        return root;
    }
};

// Definizione di NavigationManager::handleInput
inline void NavigationManager::handleInput(InputEvent event) {
    MenuPage* current = getCurrentPage();
    if (!current) return;
    
    if (event == InputEvent::BACK) {
        navigateBack();
    } else if (event == InputEvent::ENTER) {
        size_t idx = current->getSelectedIndex();
        if (idx < current->getItemsCount()) {
            MenuItem* item = current->getItem(idx);
            if (item->getType() == MenuItemType::SUBMENU) {
                SubMenuItem* sub = static_cast<SubMenuItem*>(item);
                MenuPage* newPage = sub->createPage();
                pushPage(newPage);
            } else {
                item->handleInput(event);
            }
        }
    } else {
        current->handleInput(event);
    }
    
    if (getCurrentPage()) getCurrentPage()->forceRedraw();
}

#endif