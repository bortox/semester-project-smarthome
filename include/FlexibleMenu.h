/**
 * @file FlexibleMenu.h
 * @brief Dynamic menu system with JIT page allocation and scene integration
 * @author Andrea Bortolotti
 * @version 2.0
 * @ingroup UI
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
#include "Scenes.h"

class MenuPage;
class NavigationManager;
class SubMenuItem;

/**
 * @brief Base menu item type discriminator
 * @ingroup UI
 */
enum class MenuItemType : uint8_t {
    GENERIC,
    SUBMENU
};

/**
 * @brief Abstract base class for all menu items
 * @ingroup UI
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
    /**
     * @brief Helper to print RAM string to LCD
     * @param str String in RAM
     */
    void printLabel(const char* str);
    
    /**
     * @brief Helper to print Flash string to LCD
     * @param f_str String in PROGMEM
     */
    void printLabel(const __FlashStringHelper* f_str);
};

/**
 * @brief Menu page container with event-driven updates
 * @ingroup UI
 * 
 * Manages a collection of menu items with scrolling support. Listens to device
 * events and triggers redraws when related devices change.
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
    explicit MenuPage(const __FlashStringHelper* title, MenuPage* parent = nullptr);
    
    virtual ~MenuPage();

    /**
     * @brief Adds item to page
     * @param item Heap-allocated item (page takes ownership)
     */
    void addItem(MenuItem* item);
    
    /**
     * @brief Gets item by index
     * @param idx Item index
     * @return Pointer to item or nullptr if out of bounds
     */
    MenuItem* getItem(size_t idx);
    
    /**
     * @brief Gets total number of items
     * @return Item count
     */
    size_t getItemsCount() const;
    
    /**
     * @brief Gets parent page
     * @return Parent pointer or nullptr for root
     */
    MenuPage* getParent() const;
    
    /**
     * @brief Gets currently selected item index
     * @return Selected index
     */
    size_t getSelectedIndex() const;

    void draw(uint8_t row, bool selected) override;
    bool handleInput(InputEvent event) override;
    void handleEvent(EventType type, IDevice* device, int value) override;
    
    /**
     * @brief Checks if page needs redrawing
     * @return True if redraw required
     */
    bool needsRedraw() const;
    
    /**
     * @brief Clears redraw flag after rendering
     */
    void clearRedraw();
    
    /**
     * @brief Forces full page redraw
     */
    void forceRedraw();
};

/**
 * @brief Singleton menu navigation manager with JIT page allocation
 * @ingroup UI
 * 
 * Manages a stack of MenuPage instances. Implements Just-In-Time strategy.
 */
class NavigationManager {
private:
    DynamicArray<MenuPage*> _stack;
    bool _initialized;

    NavigationManager();

public:
    /**
     * @brief Gets singleton instance
     * @return Reference to navigation manager
     */
    static NavigationManager& instance();

    /**
     * @brief Marks LCD as initialized
     */
    void setLCD();

    /**
     * @brief Initializes navigation with root page
     * @param root Pre-allocated root page
     */
    void initialize(MenuPage* root);

    /**
     * @brief Pushes JIT-allocated page onto stack
     * @param page Heap-allocated page
     */
    void pushPage(MenuPage* page);

    /**
     * @brief Navigates back and frees current page
     */
    void navigateBack();

    /**
     * @brief Gets current page from top of stack
     * @return Current page or nullptr
     */
    MenuPage* getCurrentPage();

    /**
     * @brief Delegates input to current page
     * @param event Input event
     */
    void handleInput(InputEvent event);

    /**
     * @brief Updates display if current page needs redraw
     */
    void update();

    /**
     * @brief Incrementally updates cursor position
     * @param oldIndex Previous selection index
     * @param newIndex New selection index
     */
    void drawIncrementalCursor(size_t oldIndex, size_t newIndex);

    /**
     * @brief Renders full page with scrolling support
     */
    void draw();
    
    /**
     * @brief Helper to print Flash string
     * @param f_str Flash string pointer
     */
    void printLabel(const __FlashStringHelper* f_str);
};

/**
 * @brief Toggle control for light devices
 * @ingroup UI
 */
class DeviceToggleItem : public MenuItem {
private:
    IDevice* _device;
    
public:
    /**
     * @brief Constructs toggle item for device
     * @param device Light device to control
     */
    explicit DeviceToggleItem(IDevice* device);
    
    bool relatesTo(IDevice* dev) override;
    void draw(uint8_t row, bool selected) override;
    bool handleInput(InputEvent event) override;
};

/**
 * @brief Template-based value slider with pixel-perfect progress bar
 * @ingroup UI
 * @tparam DeviceType Device class with getter/setter methods
 * 
 * Displays a two-row slider with label, value, and progress bar.
 * Must remain in header due to template instantiation.
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
            static const uint8_t customChars[] PROGMEM = {
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
                0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
                0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C,
                0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E
            };
            
            uint8_t buffer[8];
            for (uint8_t i = 0; i < 5; i++) {
                memcpy_P(buffer, &customChars[i * 8], 8);
                LCDcreateChar(i, buffer);
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

    void draw(uint8_t row, bool selected) override {
        LCD_set_cursor(0, row);
        
        printLabel(_label);
        LCD_write_str(": ");
        
        uint8_t val = (_device->*_getter)();
        char buf[5];
        itoa(val, buf, 10);
        LCD_write_str(buf);
        
        uint8_t cursor_pos = strlen_P(reinterpret_cast<const char*>(_label)) + 2 + strlen(buf);
        while (cursor_pos < 20) {
            LCD_write_char(' ');
            cursor_pos++;
        }

        LCD_set_cursor(0, row + 1);
        
        uint16_t totalPixels = map(val, _min, _max, 0, 100);
        uint8_t fullBlocks = totalPixels / 5;
        uint8_t partialPixels = totalPixels % 5;
        
        for (uint8_t i = 0; i < fullBlocks && i < 20; i++) {
            LCD_write_char(0xFF);
        }
        
        if (fullBlocks < 20 && partialPixels > 0) {
            LCD_write_char(partialPixels - 1);
            fullBlocks++;
        }
        
        for (uint8_t i = fullBlocks; i < 20; i++) {
            LCD_write_char(' ');
        }
    }

    bool handleInput(InputEvent event) override {
        uint8_t current = (_device->*_getter)();
        
        if (event == InputEvent::UP) {
            uint8_t newVal;
            if (current > _max - _step) {
                newVal = _max;
            } else {
                newVal = current + _step;
            }
            (_device->*_setter)(newVal);
            return true;
            
        } else if (event == InputEvent::DOWN) {
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

template<typename DeviceType>
bool ValueSliderItem<DeviceType>::_customCharsLoaded = false;

/**
 * @brief Helper factory for slider creation with type deduction
 * @ingroup UI
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
 * @ingroup UI
 * @tparam T Object type containing the value getter method
 * 
 * Must remain in header due to template instantiation.
 */
template<typename T>
class LiveItem : public MenuItem {
private:
    const __FlashStringHelper* _label;
    T* _object;
    int16_t (T::*_getter)() const;
    const __FlashStringHelper* _unit;
    bool _isTemperature;
    IDevice* _device;

    void printValue(int16_t value) {
        char buf[8];
        if (_isTemperature) {
            int16_t whole = value / 10;
            int16_t decimal = abs(value % 10);
            
            itoa(whole, buf, 10);
            LCD_write_str(buf);
            LCD_write_char('.');
            
            buf[0] = '0' + decimal;
            buf[1] = '\0';
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
    /**
     * @brief Constructs live item for statistics display
     * @param label Display label
     * @param object Object containing getter method
     * @param getter Member function returning int16_t value
     * @param unit Unit string
     * @param isTemp True for temperature formatting
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

    void draw(uint8_t row, bool selected) override {
        LCD_set_cursor(0, row);
        LCD_write_str(selected ? "> " : "  ");
        
        if (_label) {
            printLabel(_label);
        } else if (_device) {
            printLabel(_device->name);
        }
        
        LCD_write_str(": ");
        
        int16_t value = (_object->*_getter)();
        printValue(value);
    }

    bool handleInput(InputEvent event) override { return false; }
};

/**
 * @brief Helper factory for live item creation
 * @ingroup UI
 */
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
 * @ingroup UI
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
 * @ingroup UI
 */
class LivePIRItem : public MenuItem {
private:
    PIRSensorDevice* _sensor;

public:
    /**
     * @brief Constructs PIR display item
     * @param sensor PIR sensor device
     */
    explicit LivePIRItem(PIRSensorDevice* sensor);

    bool relatesTo(IDevice* dev) override;
    void draw(uint8_t row, bool selected) override;
    bool handleInput(InputEvent event) override;
};

/**
 * @brief Light sensor calibration item
 * @ingroup UI
 */
class LightCalibrationItem : public MenuItem {
private:
    const __FlashStringHelper* _label;
    PhotoresistorSensor* _sensor;
    bool _isDark;

public:
    /**
     * @brief Constructs calibration item
     * @param label Display label
     * @param sensor Light sensor to calibrate
     * @param isDark True for dark calibration
     */
    LightCalibrationItem(const __FlashStringHelper* label, PhotoresistorSensor* sensor, bool isDark);

    void draw(uint8_t row, bool selected) override;
    bool handleInput(InputEvent event) override;
};

/**
 * @brief Action item for scrollable lists
 * @ingroup UI
 */
class ActionItem : public MenuItem {
private:
    IDevice* _device;
    const __FlashStringHelper* _label;
    void (*_action)(IDevice*, int);
    int _param;

public:
    /**
     * @brief Constructs action item
     * @param label Display label
     * @param device Target device
     * @param action Function pointer to execute
     * @param param Integer parameter for action
     */
    ActionItem(const __FlashStringHelper* label, IDevice* device, void (*action)(IDevice*, int), int param);

    void draw(uint8_t row, bool selected) override;
    bool handleInput(InputEvent event) override;
};

/**
 * @brief Submenu item with JIT page creation
 * @ingroup UI
 */
class SubMenuItem : public MenuItem {
private:
    const __FlashStringHelper* _label;
    PageBuilder _builder;
    void* _context;

public:
    /**
     * @brief Constructs JIT submenu item
     * @param label Display label
     * @param builder Function that creates page on heap
     * @param context Optional context data for builder
     */
    SubMenuItem(const __FlashStringHelper* label, PageBuilder builder, void* context);

    MenuItemType getType() const override;
    MenuPage* createPage() const;
    void draw(uint8_t row, bool selected) override;
    bool handleInput(InputEvent event) override;
};

/**
 * @brief Back navigation menu item
 * @ingroup UI
 */
class BackMenuItem : public MenuItem {
public:
    BackMenuItem();
    
    void draw(uint8_t row, bool selected) override;
    bool handleInput(InputEvent event) override;
};

/**
 * @brief Scene activation/deactivation control
 * @ingroup UI
 */
class SceneToggleItem : public MenuItem {
private:
    IScene* _scene;

public:
    /**
     * @brief Constructs scene toggle item
     * @param scene Scene to control
     */
    explicit SceneToggleItem(IScene* scene);

    void draw(uint8_t row, bool selected) override;
    bool handleInput(InputEvent event) override;
};

/**
 * @brief Factory class for building menu page hierarchies
 * @ingroup UI
 * 
 * Provides static builder functions that create menu pages on-demand.
 */
class MenuBuilder {
private:
    static void setOutsideModeAction(IDevice* d, int v);
    static void setRGBPresetAction(IDevice* d, int v);

public:
    static MenuPage* buildRedPage(void* context);
    static MenuPage* buildGreenPage(void* context);
    static MenuPage* buildBluePage(void* context);
    static MenuPage* buildBrightnessPage(void* context);
    static MenuPage* buildCustomColorPage(void* context);
    static MenuPage* buildRGBPresetsPage(void* context);
    static MenuPage* buildRGBLightPage(void* context);
    static MenuPage* buildDimmableLightPage(void* context);
    static MenuPage* buildOutsideModesPage(void* context);
    static MenuPage* buildOutsideLightPage(void* context);
    static MenuPage* buildLightsPage(void* context);
    static MenuPage* buildSensorStatsPage(void* context);
    static MenuPage* buildLightSettingsPage(void* context);
    static MenuPage* buildSensorsPage(void* context);
    static MenuPage* buildScenesPage(void* context);
    
    /**
     * @brief Builds root menu page
     * @return Heap-allocated main menu
     */
    static MenuPage* buildMainMenu();
};

#endif