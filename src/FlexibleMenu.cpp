/**
 * @file FlexibleMenu.cpp
 * @brief Implementation of dynamic menu system components
 * @author Andrea Bortolotti
 * @version 2.0
 * @ingroup UI
 */

#include "FlexibleMenu.h"

extern NightModeScene nightMode;
extern PartyScene partyMode;
extern AlarmScene alarmMode;

/**
 * @brief Prints a RAM string to the LCD
 * @param str Null-terminated string in RAM
 */
void MenuItem::printLabel(const char* str) {
    LCD_write_str(str);
}

/**
 * @brief Prints a Flash string to the LCD
 * @param f_str Flash string pointer
 */
void MenuItem::printLabel(const __FlashStringHelper* f_str) {
    char buf[21];
    strncpy_P(buf, reinterpret_cast<const char*>(f_str), 20);
    buf[20] = 0;
    LCD_write_str(buf);
}

/**
 * @brief Constructs a menu page with title and optional parent
 * @param title Flash string title displayed at top of page
 * @param parent Parent page for navigation (nullptr for root)
 */
MenuPage::MenuPage(const __FlashStringHelper* title, MenuPage* parent)
    : _title(title), _parent(parent), _selected_index(0), _scroll_offset(0), 
      _needs_redraw(true) {
    EventSystem::instance().addListener(this, EventType::DeviceStateChanged);
    EventSystem::instance().addListener(this, EventType::DeviceValueChanged);
    EventSystem::instance().addListener(this, EventType::SensorUpdated);
}

/**
 * @brief Destructor - cleans up event listeners and child items
 */
MenuPage::~MenuPage() {
    EventSystem::instance().removeListener(this);
    for (size_t i = 0; i < _items.size(); i++) {
        delete _items[i];
    }
}

/**
 * @brief Adds a menu item to this page
 * @param item Heap-allocated menu item (page takes ownership)
 */
void MenuPage::addItem(MenuItem* item) { 
    _items.add(item); 
}

/**
 * @brief Gets menu item at specified index
 * @param idx Index of item to retrieve
 * @return Pointer to menu item or nullptr if out of bounds
 */
MenuItem* MenuPage::getItem(size_t idx) { 
    return _items[idx]; 
}

/**
 * @brief Gets total number of items in this page
 * @return Item count
 */
size_t MenuPage::getItemsCount() const { 
    return _items.size(); 
}

/**
 * @brief Gets parent page for back navigation
 * @return Parent page pointer or nullptr for root
 */
// cppcheck-suppress unusedFunction
MenuPage* MenuPage::getParent() const { 
    return _parent; 
}

/**
 * @brief Gets index of currently selected item
 * @return Selected item index
 */
size_t MenuPage::getSelectedIndex() const { 
    return _selected_index; 
}

/**
 * @brief Renders this page as a menu item (for nested pages)
 * @param row LCD row to draw on (0-3)
 * @param selected True if this item is currently selected
 */
void MenuPage::draw(uint8_t row, bool selected) {
    LCD_set_cursor(0, row);
    LCD_write_str(selected ? "> " : "  ");
    printLabel(_title);
}

/**
 * @brief Handles navigation input for this page
 * @param event Input event (UP, DOWN, ENTER, BACK)
 * @return True if event was handled
 */
bool MenuPage::handleInput(InputEvent event) {
    size_t oldIndex = _selected_index;

    if (_selected_index < _items.size()) {
        bool handled = _items[_selected_index]->handleInput(event);
        if (handled) {
            _needs_redraw = true;
            return true;
        }
    }

    if (event == InputEvent::UP) {
        if (_selected_index > 0) { 
            _selected_index--; 
            
            if (_selected_index < _scroll_offset) {
                _scroll_offset = _selected_index;
                _needs_redraw = true;
            } else {
                NavigationManager::instance().drawIncrementalCursor(oldIndex, _selected_index);
                return true;
            }
        }
        return true;
    } else if (event == InputEvent::DOWN) {
        if (_selected_index < static_cast<size_t>(_items.size() - 1)) {
            _selected_index++; 
            
            if (_selected_index >= _scroll_offset + 3) {
                _scroll_offset = _selected_index - 2;
                _needs_redraw = true;
            } else {
                NavigationManager::instance().drawIncrementalCursor(oldIndex, _selected_index);
                return true;
            }
        }
        return true;
    }
    
    return false;
}

/**
 * @brief Handles device events for automatic page updates
 * @param type Event type received
 * @param device Device that triggered the event
 * @param value Event-specific value
 */
void MenuPage::handleEvent(EventType type, IDevice* device, int value) {
    static_cast<void>(type);
    static_cast<void>(value);
    
    const MenuPage* currentPage = NavigationManager::instance().getCurrentPage();
    if (currentPage != this) return;
    
    for (size_t i = 0; i < _items.size(); i++) {
        if (_items[i]->relatesTo(device)) {
            _needs_redraw = true;
            return;
        }
    }
}

/**
 * @brief Checks if page needs to be redrawn
 * @return True if redraw is required
 */
bool MenuPage::needsRedraw() const { 
    return _needs_redraw; 
}

/**
 * @brief Clears the redraw flag after rendering
 */
void MenuPage::clearRedraw() { 
    _needs_redraw = false; 
}

/**
 * @brief Forces a full page redraw on next update
 */
void MenuPage::forceRedraw() { 
    _needs_redraw = true; 
}

/**
 * @brief Private constructor for singleton pattern
 */
NavigationManager::NavigationManager() : _initialized(false) {}

/**
 * @brief Gets the singleton instance
 * @return Reference to NavigationManager instance
 */
NavigationManager& NavigationManager::instance() {
    static NavigationManager inst;
    return inst;
}

/**
 * @brief Marks LCD as initialized and ready for use
 */
// cppcheck-suppress unusedFunction
void NavigationManager::setLCD() { 
    _initialized = true; 
}

/**
 * @brief Initializes navigation with root menu page
 * @param root Pre-allocated root menu page
 */
// cppcheck-suppress unusedFunction
void NavigationManager::initialize(MenuPage* root) {
    _stack.add(root);
    draw();
}

/**
 * @brief Pushes a new page onto the navigation stack
 * @param page Heap-allocated page to display
 */
void NavigationManager::pushPage(MenuPage* page) {
    if (page) {
        _stack.add(page);
        page->forceRedraw();
        draw();
    }
}

/**
 * @brief Navigates back to previous page and frees current page
 */
void NavigationManager::navigateBack() {
    if (_stack.size() > 1) {
        MenuPage* current = _stack[_stack.size() - 1];
        _stack.remove(_stack.size() - 1);
        delete current;
        
        getCurrentPage()->forceRedraw();
        draw();
    }
}

/**
 * @brief Gets the currently displayed page
 * @return Pointer to current page or nullptr if stack empty
 */
MenuPage* NavigationManager::getCurrentPage() {
    if (_stack.size() > 0) return _stack[_stack.size() - 1];
    return nullptr;
}

/**
 * @brief Delegates input event to current page
 * @param event Input event to handle
 */
void NavigationManager::handleInput(InputEvent event) {
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

/**
 * @brief Updates display if current page needs redrawing
 */
void NavigationManager::update() {
    MenuPage* current = getCurrentPage();
    if (current && current->needsRedraw()) {
        draw();
        current->clearRedraw();
    }
}

/**
 * @brief Incrementally updates cursor without full redraw
 * @param oldIndex Previous selection index
 * @param newIndex New selection index
 */
void NavigationManager::drawIncrementalCursor(size_t oldIndex, size_t newIndex) {
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
 * @brief Renders full page with title, items, and scroll indicators
 */
void NavigationManager::draw() {
    MenuPage* current = getCurrentPage();
    if (!current || !_initialized) return;

    LCD_clear();
    LCD_set_cursor(0, 0);
    printLabel(current->_title);
    
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

/**
 * @brief Helper to print Flash string to LCD
 * @param f_str Flash string pointer
 */
void NavigationManager::printLabel(const __FlashStringHelper* f_str) {
    char buf[21];
    strncpy_P(buf, reinterpret_cast<const char*>(f_str), 20);
    buf[20] = 0;
    LCD_write_str(buf);
}

/**
 * @brief Constructs device toggle item
 * @param device Device to control
 */
DeviceToggleItem::DeviceToggleItem(IDevice* device) : _device(device) {}

/**
 * @brief Checks if this item relates to specified device
 * @param dev Device to check
 * @return True if this item controls the device
 */
bool DeviceToggleItem::relatesTo(IDevice* dev) { 
    return _device == dev; 
}

/**
 * @brief Renders toggle item with device name and state
 * @param row LCD row to draw on
 * @param selected True if item is selected
 */
void DeviceToggleItem::draw(uint8_t row, bool selected) {
    LCD_set_cursor(0, row);
    LCD_write_str(selected ? "> " : "  ");
    printLabel(_device->name);
    LCD_set_cursor(15, row);
    
    if (_device->isLight()) {
        const SimpleLight* light = static_cast<const SimpleLight*>(_device);
        LCD_write_str(light->getState() ? "ON " : "OFF");
    }
}

/**
 * @brief Handles input for device toggle
 * @param event Input event
 * @return True if event was handled
 */
bool DeviceToggleItem::handleInput(InputEvent event) {
    if (event == InputEvent::ENTER && _device->isLight()) {
        static_cast<SimpleLight*>(_device)->toggle();
        return true;
    }
    return false;
}

/**
 * @brief Constructs PIR live display item
 * @param sensor PIR sensor device to display
 */
LivePIRItem::LivePIRItem(PIRSensorDevice* sensor) : _sensor(sensor) {}

/**
 * @brief Checks if this item relates to specified device
 * @param dev Device to check
 * @return True if this item displays the device
 */
bool LivePIRItem::relatesTo(IDevice* dev) { 
    return _sensor == dev; 
}

/**
 * @brief Renders PIR status with motion detection state
 * @param row LCD row to draw on
 * @param selected True if item is selected
 */
void LivePIRItem::draw(uint8_t row, bool selected) {
    LCD_set_cursor(0, row);
    LCD_write_str(selected ? "> " : "  ");
    printLabel(_sensor->name);
    LCD_write_str(": ");
    LCD_write_str(_sensor->isMotionDetected() ? "Yes" : "No");
}

/**
 * @brief Handles input for PIR item (no action)
 * @param event Input event
 * @return Always false (read-only item)
 */
bool LivePIRItem::handleInput(InputEvent event) { 
    static_cast<void>(event);
    return false; 
}

/**
 * @brief Constructs light calibration item
 * @param label Display label
 * @param sensor Light sensor to calibrate
 * @param isDark True for dark limit, false for bright limit
 */
LightCalibrationItem::LightCalibrationItem(const __FlashStringHelper* label, PhotoresistorSensor* sensor, bool isDark)
    : _label(label), _sensor(sensor), _isDark(isDark) {}

/**
 * @brief Renders calibration item
 * @param row LCD row to draw on
 * @param selected True if item is selected
 */
void LightCalibrationItem::draw(uint8_t row, bool selected) {
    LCD_set_cursor(0, row);
    LCD_write_str(selected ? "> " : "  ");
    printLabel(_label);
}

/**
 * @brief Handles calibration action on ENTER
 * @param event Input event
 * @return True if calibration was performed
 */
bool LightCalibrationItem::handleInput(InputEvent event) {
    if (event == InputEvent::ENTER) {
        if (_isDark) {
            _sensor->calibrateCurrentAsMin();
        } else {
            _sensor->calibrateCurrentAsMax();
        }
        _sensor->getStats().reset();
        EventSystem::instance().emit(EventType::SensorUpdated, _sensor, _sensor->getValue());
        return true;
    }
    return false;
}

/**
 * @brief Constructs action item with callback
 * @param label Display label
 * @param device Target device for action
 * @param action Function pointer to execute
 * @param param Parameter to pass to action
 */
ActionItem::ActionItem(const __FlashStringHelper* label, IDevice* device, void (*action)(IDevice*, int), int param)
    : _device(device), _label(label), _action(action), _param(param) {}

/**
 * @brief Renders action item
 * @param row LCD row to draw on
 * @param selected True if item is selected
 */
void ActionItem::draw(uint8_t row, bool selected) {
    LCD_set_cursor(0, row);
    LCD_write_str(selected ? "> " : "  ");
    printLabel(_label);
}

/**
 * @brief Executes action and navigates back on ENTER
 * @param event Input event
 * @return True if action was executed
 */
bool ActionItem::handleInput(InputEvent event) {
    if (event == InputEvent::ENTER) {
        _action(_device, _param);
        NavigationManager::instance().navigateBack();
        return true;
    }
    return false;
}

/**
 * @brief Constructs submenu item with JIT page builder
 * @param label Display label
 * @param builder Function that creates the submenu page
 * @param context Context data passed to builder
 */
SubMenuItem::SubMenuItem(const __FlashStringHelper* label, PageBuilder builder, void* context)
    : _label(label), _builder(builder), _context(context) {}

/**
 * @brief Returns item type for polymorphic dispatch
 * @return MenuItemType::SUBMENU
 */
MenuItemType SubMenuItem::getType() const { 
    return MenuItemType::SUBMENU; 
}

/**
 * @brief Creates submenu page using JIT builder
 * @return Heap-allocated menu page
 */
MenuPage* SubMenuItem::createPage() const { 
    return _builder(_context); 
}

/**
 * @brief Renders submenu item with arrow indicator
 * @param row LCD row to draw on
 * @param selected True if item is selected
 */
void SubMenuItem::draw(uint8_t row, bool selected) {
    LCD_set_cursor(0, row);
    LCD_write_str(selected ? "> " : "  ");
    printLabel(_label);
    LCD_write_str(" >");
}

/**
 * @brief Handles submenu navigation on ENTER
 * @param event Input event
 * @return True if navigation occurred
 */
bool SubMenuItem::handleInput(InputEvent event) {
    if (event == InputEvent::ENTER) {
        MenuPage* newPage = createPage();
        NavigationManager::instance().pushPage(newPage);
        return true;
    }
    return false;
}

/**
 * @brief Constructs back navigation item
 */
BackMenuItem::BackMenuItem() {}

/**
 * @brief Renders back item
 * @param row LCD row to draw on
 * @param selected True if item is selected
 */
void BackMenuItem::draw(uint8_t row, bool selected) {
    LCD_set_cursor(0, row);
    LCD_write_str(selected ? "> " : "  ");
    LCD_write_str("<< Back");
}

/**
 * @brief Handles back navigation on ENTER
 * @param event Input event
 * @return True if navigation occurred
 */
bool BackMenuItem::handleInput(InputEvent event) {
    if (event == InputEvent::ENTER) {
        NavigationManager::instance().navigateBack();
        return true;
    }
    return false;
}

/**
 * @brief Constructs scene toggle item
 * @param scene Scene to control
 */
SceneToggleItem::SceneToggleItem(IScene* scene) : _scene(scene) {}

/**
 * @brief Renders scene item with activation state
 * @param row LCD row to draw on
 * @param selected True if item is selected
 */
void SceneToggleItem::draw(uint8_t row, bool selected) {
    LCD_set_cursor(0, row);
    LCD_write_str(selected ? "> " : "  ");
    LCD_write_str(_scene->getName());
    LCD_set_cursor(15, row);
    LCD_write_str(_scene->isActive() ? "ON " : "OFF");
}

/**
 * @brief Toggles scene activation on ENTER
 * @param event Input event
 * @return True if scene state changed
 */
bool SceneToggleItem::handleInput(InputEvent event) {
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

/**
 * @brief Action callback to set outside light mode
 * @param d Device pointer (OutsideLight)
 * @param v Mode value as integer
 */
void MenuBuilder::setOutsideModeAction(IDevice* d, int v) {
    static_cast<OutsideLight*>(d)->setMode(static_cast<OutsideMode>(v));
}

/**
 * @brief Action callback to set RGB preset color
 * @param d Device pointer (RGBLight)
 * @param v Preset value as integer
 */
void MenuBuilder::setRGBPresetAction(IDevice* d, int v) {
    static_cast<RGBLight*>(d)->setPreset(static_cast<RGBPreset>(v));
}

/**
 * @brief Builds red channel adjustment page
 * @param context RGBLight pointer
 * @return Heap-allocated menu page
 */
MenuPage* MenuBuilder::buildRedPage(void* context) {
    RGBLight* light = static_cast<RGBLight*>(context);
    MenuPage* page = new MenuPage(F("Red Channel"), nullptr);
    if (!page) return nullptr;
    
    page->addItem(makeSlider(light, F("Red"), &RGBLight::getRed, &RGBLight::setRed, 0, 255, 3));
    page->addItem(new BackMenuItem());
    return page;
}

/**
 * @brief Builds green channel adjustment page
 * @param context RGBLight pointer
 * @return Heap-allocated menu page
 */
MenuPage* MenuBuilder::buildGreenPage(void* context) {
    RGBLight* light = static_cast<RGBLight*>(context);
    MenuPage* page = new MenuPage(F("Green Channel"), nullptr);
    if (!page) return nullptr;
    
    page->addItem(makeSlider(light, F("Green"), &RGBLight::getGreen, &RGBLight::setGreen, 0, 255, 3));
    page->addItem(new BackMenuItem());
    return page;
}

/**
 * @brief Builds blue channel adjustment page
 * @param context RGBLight pointer
 * @return Heap-allocated menu page
 */
MenuPage* MenuBuilder::buildBluePage(void* context) {
    RGBLight* light = static_cast<RGBLight*>(context);
    MenuPage* page = new MenuPage(F("Blue Channel"), nullptr);
    if (!page) return nullptr;
    
    page->addItem(makeSlider(light, F("Blue"), &RGBLight::getBlue, &RGBLight::setBlue, 0, 255, 3));
    page->addItem(new BackMenuItem());
    return page;
}

/**
 * @brief Builds brightness adjustment page
 * @param context DimmableLight pointer
 * @return Heap-allocated menu page
 */
MenuPage* MenuBuilder::buildBrightnessPage(void* context) {
    DimmableLight* light = static_cast<DimmableLight*>(context);
    MenuPage* page = new MenuPage(F("Brightness"), nullptr);
    if (!page) return nullptr;
    
    page->addItem(makeSlider(light, F("Level"), &DimmableLight::getBrightness, &DimmableLight::setBrightness, 0, 100, 15));
    return page;
}

/**
 * @brief Builds custom color submenu page
 * @param context RGBLight pointer
 * @return Heap-allocated menu page
 */
MenuPage* MenuBuilder::buildCustomColorPage(void* context) {
    RGBLight* light = static_cast<RGBLight*>(context);
    MenuPage* page = new MenuPage(F("Custom Color"), nullptr);
    if (!page) return nullptr;

    page->addItem(new SubMenuItem(F("Set Red"), buildRedPage, light));
    page->addItem(new SubMenuItem(F("Set Green"), buildGreenPage, light));
    page->addItem(new SubMenuItem(F("Set Blue"), buildBluePage, light));
    page->addItem(new BackMenuItem());

    return page;
}

/**
 * @brief Builds RGB presets selection page
 * @param context RGBLight pointer
 * @return Heap-allocated menu page
 */
MenuPage* MenuBuilder::buildRGBPresetsPage(void* context) {
    RGBLight* light = static_cast<RGBLight*>(context);
    MenuPage* page = new MenuPage(F("Select Preset"), NavigationManager::instance().getCurrentPage());
    if (!page) return nullptr;

    page->addItem(new ActionItem(F("Warm White"), light, setRGBPresetAction, static_cast<int>(RGBPreset::WARM_WHITE)));
    page->addItem(new ActionItem(F("Cool White"), light, setRGBPresetAction, static_cast<int>(RGBPreset::COOL_WHITE)));
    page->addItem(new ActionItem(F("Red"), light, setRGBPresetAction, static_cast<int>(RGBPreset::RED)));
    page->addItem(new ActionItem(F("Green"), light, setRGBPresetAction, static_cast<int>(RGBPreset::GREEN)));
    page->addItem(new ActionItem(F("Blue"), light, setRGBPresetAction, static_cast<int>(RGBPreset::BLUE)));
    page->addItem(new ActionItem(F("Ocean"), light, setRGBPresetAction, static_cast<int>(RGBPreset::OCEAN)));
    page->addItem(new BackMenuItem());
    return page;
}

/**
 * @brief Builds RGB light control page
 * @param context RGBLight pointer
 * @return Heap-allocated menu page
 */
MenuPage* MenuBuilder::buildRGBLightPage(void* context) {
    RGBLight* light = static_cast<RGBLight*>(context);
    MenuPage* page = new MenuPage(F("RGB Light"), NavigationManager::instance().getCurrentPage());
    if (!page) return nullptr;

    page->addItem(new DeviceToggleItem(light));
    page->addItem(new SubMenuItem(F("Set Brightness"), buildBrightnessPage, light));
    page->addItem(new SubMenuItem(F("Color Presets"), buildRGBPresetsPage, light));
    page->addItem(new SubMenuItem(F("Custom Color"), buildCustomColorPage, light));
    page->addItem(new BackMenuItem());
    return page;
}

/**
 * @brief Builds dimmable light control page
 * @param context DimmableLight pointer
 * @return Heap-allocated menu page
 */
MenuPage* MenuBuilder::buildDimmableLightPage(void* context) {
    DimmableLight* light = static_cast<DimmableLight*>(context);
    MenuPage* page = new MenuPage(F("Dimmable Light"), NavigationManager::instance().getCurrentPage());
    if (!page) return nullptr;

    page->addItem(new DeviceToggleItem(light));
    page->addItem(new SubMenuItem(F("Set Brightness"), buildBrightnessPage, light));
    page->addItem(new BackMenuItem());
    return page;
}

/**
 * @brief Builds outside light modes selection page
 * @param context OutsideLight pointer
 * @return Heap-allocated menu page
 */
MenuPage* MenuBuilder::buildOutsideModesPage(void* context) {
    OutsideLight* light = static_cast<OutsideLight*>(context);
    MenuPage* page = new MenuPage(F("Select Mode"), NavigationManager::instance().getCurrentPage());
    if (!page) return nullptr;

    page->addItem(new ActionItem(F("OFF"), light, setOutsideModeAction, static_cast<int>(OutsideMode::OFF)));
    page->addItem(new ActionItem(F("ON"), light, setOutsideModeAction, static_cast<int>(OutsideMode::ON)));
    page->addItem(new ActionItem(F("AUTO LIGHT"), light, setOutsideModeAction, static_cast<int>(OutsideMode::AUTO_LIGHT)));
    page->addItem(new ActionItem(F("AUTO MOTION"), light, setOutsideModeAction, static_cast<int>(OutsideMode::AUTO_MOTION)));
    page->addItem(new BackMenuItem());
    return page;
}

/**
 * @brief Builds outside light control page
 * @param context OutsideLight pointer
 * @return Heap-allocated menu page
 */
MenuPage* MenuBuilder::buildOutsideLightPage(void* context) {
    OutsideLight* light = static_cast<OutsideLight*>(context);
    MenuPage* page = new MenuPage(F("Outside Light"), NavigationManager::instance().getCurrentPage());
    if (!page) return nullptr;

    page->addItem(new SubMenuItem(F("Set Mode"), buildOutsideModesPage, light));
    page->addItem(new BackMenuItem());
    return page;
}

/**
 * @brief Builds lights listing page with all light devices
 * @param context Unused
 * @return Heap-allocated menu page
 */
MenuPage* MenuBuilder::buildLightsPage(void* context) {
    static_cast<void>(context);
    MenuPage* page = new MenuPage(F("Lights"), NavigationManager::instance().getCurrentPage());
    if (!page) return nullptr;

    DeviceRegistry& registry = DeviceRegistry::instance();
    const DynamicArray<IDevice*>& devices = registry.getDevices();
    
    for (size_t i = 0; i < devices.size(); i++) {
        IDevice* d = devices[i];
        if (d->type == DeviceType::LightSimple) {
            page->addItem(new DeviceToggleItem(d));
        } else if (d->type == DeviceType::LightDimmable) {
            page->addItem(new SubMenuItem(d->name, buildDimmableLightPage, d));
        } else if (d->type == DeviceType::LightRGB) {
            page->addItem(new SubMenuItem(d->name, buildRGBLightPage, d));
        } else if (d->type == DeviceType::LightOutside) {
            page->addItem(new SubMenuItem(d->name, buildOutsideLightPage, d));
        }
    }
    page->addItem(new BackMenuItem());
    return page;
}

/**
 * @brief Builds sensor statistics page
 * @param context IDevice pointer (sensor)
 * @return Heap-allocated menu page
 */
MenuPage* MenuBuilder::buildSensorStatsPage(void* context) {
    IDevice* device = static_cast<IDevice*>(context);
    MenuPage* page = new MenuPage(F("Statistics"), NavigationManager::instance().getCurrentPage());
    if (!page) return nullptr;
    
    if (device->type == DeviceType::SensorTemperature) {
        TemperatureSensor* temp = static_cast<TemperatureSensor*>(device);
        SensorStats* stats = &temp->getStats();
        
        page->addItem(makeLiveItem(device, temp, &TemperatureSensor::getTemperature, F("C"), true));
        page->addItem(makeLiveItem(F("Min"), stats, &SensorStats::getMin, F("C"), true));
        page->addItem(makeLiveItem(F("Max"), stats, &SensorStats::getMax, F("C"), true));
        page->addItem(makeLiveItem(F("Avg"), stats, &SensorStats::getAverage, F("C"), true));
        
    } else if (device->type == DeviceType::SensorLight) {
        PhotoresistorSensor* light = static_cast<PhotoresistorSensor*>(device);
        SensorStats* stats = &light->getStats();
        
        page->addItem(makeLiveItem(device, light, 
            static_cast<int16_t (PhotoresistorSensor::*)() const>(&PhotoresistorSensor::getValue), 
            F("%"), false));
        page->addItem(makeLiveItem(F("Min"), stats, &SensorStats::getMin, F("%"), false));
        page->addItem(makeLiveItem(F("Max"), stats, &SensorStats::getMax, F("%"), false));
        page->addItem(makeLiveItem(F("Avg"), stats, &SensorStats::getAverage, F("%"), false));
        
    } else if (device->type == DeviceType::SensorRAM) {
        RamSensorDevice* ram = static_cast<RamSensorDevice*>(device);
        SensorStats* stats = &ram->getStats();
        
        page->addItem(makeLiveItem(device, ram, &RamSensorDevice::getValue, F("B"), false));
        page->addItem(makeLiveItem(F("Min"), stats, &SensorStats::getMin, F("B"), false));
        page->addItem(makeLiveItem(F("Max"), stats, &SensorStats::getMax, F("B"), false));
        page->addItem(makeLiveItem(F("Avg"), stats, &SensorStats::getAverage, F("B"), false));
        
    } else if (device->type == DeviceType::SensorVCC) {
        VccSensorDevice* vcc = static_cast<VccSensorDevice*>(device);
        SensorStats* stats = &vcc->getStats();
        
        page->addItem(makeLiveItem(device, vcc, &VccSensorDevice::getValue, F("mV"), false));
        page->addItem(makeLiveItem(F("Min"), stats, &SensorStats::getMin, F("mV"), false));
        page->addItem(makeLiveItem(F("Max"), stats, &SensorStats::getMax, F("mV"), false));
        page->addItem(makeLiveItem(F("Avg"), stats, &SensorStats::getAverage, F("mV"), false));
        
    } else if (device->type == DeviceType::SensorLoopTime) {
        LoopTimeSensorDevice* loopSensor = static_cast<LoopTimeSensorDevice*>(device);
        SensorStats* stats = &loopSensor->getStats();
        
        page->addItem(makeLiveItem(device, loopSensor, &LoopTimeSensorDevice::getValue, F("us"), false));
        page->addItem(makeLiveItem(F("Min"), stats, &SensorStats::getMin, F("us"), false));
        page->addItem(makeLiveItem(F("Max"), stats, &SensorStats::getMax, F("us"), false));
        page->addItem(makeLiveItem(F("Avg"), stats, &SensorStats::getAverage, F("us"), false));
    }
    
    page->addItem(new BackMenuItem());
    return page;
}

/**
 * @brief Builds light sensor settings page with calibration
 * @param context PhotoresistorSensor pointer
 * @return Heap-allocated menu page
 */
MenuPage* MenuBuilder::buildLightSettingsPage(void* context) {
    PhotoresistorSensor* light = static_cast<PhotoresistorSensor*>(context);
    MenuPage* page = new MenuPage(F("Light Settings"), NavigationManager::instance().getCurrentPage());
    if (!page) return nullptr;
    
    page->addItem(new SubMenuItem(F("View Stats"), buildSensorStatsPage, light));
    page->addItem(new LightCalibrationItem(F("Set Dark Limit"), light, true));
    page->addItem(new LightCalibrationItem(F("Set Bright Limit"), light, false));
    page->addItem(new BackMenuItem());
    return page;
}

/**
 * @brief Builds sensors listing page with all sensor devices
 * @param context Unused
 * @return Heap-allocated menu page
 */
MenuPage* MenuBuilder::buildSensorsPage(void* context) {
    static_cast<void>(context);
    MenuPage* page = new MenuPage(F("Sensors"), NavigationManager::instance().getCurrentPage());
    if (!page) return nullptr;

    DeviceRegistry& registry = DeviceRegistry::instance();
    const DynamicArray<IDevice*>& devices = registry.getDevices();
    
    for (size_t i = 0; i < devices.size(); i++) {
        IDevice* d = devices[i];
        if (d->isSensor()) {
            if (d->type == DeviceType::SensorTemperature) {
                page->addItem(new SubMenuItem(F("Temperature"), buildSensorStatsPage, d));
                
            } else if (d->type == DeviceType::SensorLight) {
                page->addItem(new SubMenuItem(F("Light Sensor"), buildLightSettingsPage, d));
                
            } else if (d->type == DeviceType::SensorPIR) {
                page->addItem(new LivePIRItem(static_cast<PIRSensorDevice*>(d)));
                
            } else if (d->type == DeviceType::SensorRAM) {
                page->addItem(new SubMenuItem(F("Free RAM"), buildSensorStatsPage, d));
                
            } else if (d->type == DeviceType::SensorVCC) {
                page->addItem(new SubMenuItem(F("VCC Voltage"), buildSensorStatsPage, d));
                
            } else if (d->type == DeviceType::SensorLoopTime) {
                page->addItem(new SubMenuItem(F("Loop Time"), buildSensorStatsPage, d));
            }
        }
    }
    
    page->addItem(new BackMenuItem());
    return page;
}

/**
 * @brief Builds scenes control page
 * @param context Unused
 * @return Heap-allocated menu page
 */
MenuPage* MenuBuilder::buildScenesPage(void* context) {
    static_cast<void>(context);
    MenuPage* page = new MenuPage(F("Scenes"), NavigationManager::instance().getCurrentPage());
    if (!page) return nullptr;
    
    page->addItem(new SceneToggleItem(&nightMode));
    page->addItem(new SceneToggleItem(&partyMode));
    page->addItem(new SceneToggleItem(&alarmMode));
    
    page->addItem(new BackMenuItem());
    return page;
}

/**
 * @brief Builds main menu root page
 * @return Heap-allocated root menu page
 */
// cppcheck-suppress unusedFunction
MenuPage* MenuBuilder::buildMainMenu() {
    MenuPage* root = new MenuPage(F("Main Menu"));
    if (!root) return nullptr;

    root->addItem(new SubMenuItem(F("Lights"), buildLightsPage, nullptr));
    root->addItem(new SubMenuItem(F("Sensors"), buildSensorsPage, nullptr));
    root->addItem(new SubMenuItem(F("Scenes"), buildScenesPage, nullptr));
    return root;
}
