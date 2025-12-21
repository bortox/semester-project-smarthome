/**
 * @file CoreSystem.h
 * @brief Core system infrastructure for event-driven embedded architecture
 * @author Andrea Bortolotti
 * @version 2.0
 * 
 * @details Provides the foundational components for the smart home system:
 * - Dynamic array container (template-based, header-only)
 * - Event system for decoupled communication
 * - Device registry for centralized device management
 * - Base device interface
 * 
 * @note This file must remain header-only due to template classes.
 * 
 * @ingroup Core
 */
#ifndef CORE_SYSTEM_H
#define CORE_SYSTEM_H

#include <Arduino.h>

/**
 * @brief Device type discriminator for polymorphic dispatch
 * @ingroup Core
 */
enum class DeviceType : uint8_t {
    Unknown,            ///< Default/uninitialized device type
    LightSimple,        ///< Simple on/off light
    LightDimmable,      ///< Light with brightness control
    LightRGB,           ///< RGB color-controllable light
    LightOutside,       ///< Outdoor light with automation
    SensorTemperature,  ///< Temperature sensor (LM75)
    SensorLight,        ///< Photoresistor light sensor
    SensorPIR,          ///< Passive infrared motion sensor
    SensorRAM,          ///< Free RAM monitor
    SensorVCC,          ///< Supply voltage monitor
    SensorLoopTime      ///< Loop execution time monitor
};

/**
 * @brief Event types for inter-component communication
 * @ingroup Core
 * 
 * @details Strongly-typed enum for type safety. Used with EventSystem
 * to enable decoupled Observer pattern communication between components.
 */
enum class EventType : uint8_t {
    ButtonPressed,        ///< Physical button press detected
    DeviceStateChanged,   ///< Device on/off state changed
    DeviceValueChanged,   ///< Device value (brightness, color) changed
    SensorUpdated         ///< Sensor reading updated
};

/**
 * @brief Input event types for menu navigation
 * @ingroup Core
 */
enum class InputEvent : uint8_t {
    NONE,   ///< No input detected
    UP,     ///< Navigate up in menu
    DOWN,   ///< Navigate down in menu
    ENTER,  ///< Select/confirm action
    BACK    ///< Return to previous menu
};

class MenuPage;

/**
 * @brief Function pointer type for JIT menu page creation
 * @param context User-defined context data (typically device pointer)
 * @return Heap-allocated MenuPage instance
 * @ingroup UI
 */
typedef MenuPage* (*PageBuilder)(void* context);

/**
 * @class DynamicArray
 * @brief Memory-efficient dynamic array for embedded systems
 * @tparam T Element type (typically pointers)
 * @ingroup Core
 * 
 * @details Template-based container optimized for AVR microcontrollers.
 * Grows dynamically as needed with configurable growth factor.
 * Must remain header-only due to template instantiation requirements.
 * 
 * @note Memory is allocated from heap. Monitor free RAM when using.
 */
template<typename T>
class DynamicArray {
private:
    T* _data;           ///< Pointer to element storage
    uint8_t _size;      ///< Current number of elements
    uint8_t _capacity;  ///< Allocated storage capacity

public:
    /**
     * @brief Default constructor
     * @details Initializes empty array with no allocation
     */
    DynamicArray() : _data(nullptr), _size(0), _capacity(0) {}
    
    /**
     * @brief Destructor
     * @details Frees allocated memory. Does NOT delete pointed-to objects.
     */
    ~DynamicArray() { 
        if (_data) free(_data); 
    }

    /**
     * @brief Adds element to end of array
     * @param item Element to add
     * @return true if successful, false if memory allocation failed
     */
    bool add(T item) {
        if (_size >= _capacity) {
            uint8_t newCap = _capacity == 0 ? 4 : _capacity + 4;
            T* newData = static_cast<T*>(realloc(_data, newCap * sizeof(T)));
            if (!newData) return false;
            _data = newData;
            _capacity = newCap;
        }
        _data[_size++] = item;
        return true;
    }

    /**
     * @brief Removes element at specified index
     * @param index Position of element to remove
     * @details Shifts subsequent elements left to fill gap
     */
    void remove(uint8_t index) {
        if (index < _size) {
            for (uint8_t i = index; i < _size - 1; i++) {
                _data[i] = _data[i + 1];
            }
            _size--;
        }
    }

    /**
     * @brief Clears all elements
     * @details Resets size to zero but does not free memory
     */
    void clear() { _size = 0; }

    /**
     * @brief Array subscript operator
     * @param index Element index
     * @return Reference to element at index
     * @warning No bounds checking performed
     */
    T& operator[](uint8_t index) { return _data[index]; }
    
    /**
     * @brief Const array subscript operator
     * @param index Element index
     * @return Const reference to element at index
     * @warning No bounds checking performed
     */
    const T& operator[](uint8_t index) const { return _data[index]; }

    /**
     * @brief Gets current element count
     * @return Number of elements in array
     */
    uint8_t size() const { return _size; }
};

class IDevice;

/**
 * @class IEventListener
 * @brief Interface for event-driven components
 * @ingroup Core
 * 
 * @details Implements Observer pattern. Classes inheriting this interface
 * can subscribe to specific event types and receive notifications.
 */
class IEventListener {
public:
    /**
     * @brief Virtual destructor for proper cleanup
     */
    virtual ~IEventListener() {}
    
    /**
     * @brief Event handler callback
     * @param type Type of event received
     * @param device Source device that triggered the event
     * @param value Event-specific value (interpretation depends on event type)
     */
    virtual void handleEvent(EventType type, IDevice* device, int value) = 0;
};

/**
 * @class EventSystem
 * @brief Singleton event bus for decoupled communication
 * @ingroup Core
 * 
 * @details Implements publish-subscribe pattern. Components register
 * as listeners for specific event types and receive callbacks when
 * those events are emitted.
 * 
 * @note Maximum 8 event types supported (size of EventType enum)
 */
class EventSystem {
private:
    /**
     * @brief Internal listener registration structure
     */
    struct ListenerEntry {
        IEventListener* listener;  ///< Pointer to listener object
        EventType type;            ///< Event type filter
    };
    
    DynamicArray<ListenerEntry> _listeners;  ///< Registered listeners

    /**
     * @brief Private constructor for singleton pattern
     */
    EventSystem() {}

public:
    /**
     * @brief Gets singleton instance
     * @return Reference to EventSystem instance
     */
    static EventSystem& instance() {
        static EventSystem inst;
        return inst;
    }

    /**
     * @brief Registers listener for specific event type
     * @param listener Pointer to listener object
     * @param type Event type to listen for
     */
    void addListener(IEventListener* listener, EventType type) {
        _listeners.add({listener, type});
    }

    /**
     * @brief Unregisters listener from all event types
     * @param listener Pointer to listener object to remove
     */
    void removeListener(const IEventListener* listener) {
        for (uint8_t i = 0; i < _listeners.size(); ) {
            if (_listeners[i].listener == listener) {
                _listeners.remove(i);
            } else {
                i++;
            }
        }
    }

    /**
     * @brief Emits event to all registered listeners
     * @param type Event type
     * @param source Device that triggered the event
     * @param value Event-specific value
     */
    void emit(EventType type, IDevice* source, int value) {
        for (uint8_t i = 0; i < _listeners.size(); i++) {
            if (_listeners[i].type == type) {
                _listeners[i].listener->handleEvent(type, source, value);
            }
        }
    }
};

/**
 * @class IDevice
 * @brief Abstract base class for all controllable devices
 * @ingroup Core
 * 
 * @details Provides common interface for device identification,
 * type checking, and periodic updates. All concrete device classes
 * must inherit from this interface.
 */
class IDevice {
public:
    const __FlashStringHelper* name;  ///< Device name stored in Flash memory
    const DeviceType type;            ///< Device type discriminator

    /**
     * @brief Constructor
     * @param n Device name (Flash string)
     * @param t Device type
     */
    IDevice(const __FlashStringHelper* n, DeviceType t) : name(n), type(t) {}
    
    /**
     * @brief Virtual destructor
     */
    virtual ~IDevice() {}

    /**
     * @brief Periodic update callback
     * @details Called from main loop. Implementations should be non-blocking.
     */
    virtual void update() = 0;

    /**
     * @brief Checks if device is a light
     * @return true if device is any light type
     */
    virtual bool isLight() const { return false; }
    
    /**
     * @brief Checks if device is a sensor
     * @return true if device is any sensor type
     */
    virtual bool isSensor() const { return false; }
};

/**
 * @class DeviceRegistry
 * @brief Singleton registry for all system devices
 * @ingroup Core
 * 
 * @details Maintains central list of all registered devices.
 * Provides access for iteration (menu building) and lookup.
 */
class DeviceRegistry {
private:
    DynamicArray<IDevice*> _devices;  ///< Registered device pointers

    /**
     * @brief Private constructor for singleton pattern
     */
    DeviceRegistry() {}

public:
    /**
     * @brief Gets singleton instance
     * @return Reference to DeviceRegistry instance
     */
    static DeviceRegistry& instance() {
        static DeviceRegistry inst;
        return inst;
    }

    /**
     * @brief Registers device with registry
     * @param device Pointer to device to register
     */
    void registerDevice(IDevice* device) { 
        _devices.add(device); 
    }

    /**
     * @brief Unregisters device from registry
     * @param device Pointer to device to unregister
     */
    void unregisterDevice(const IDevice* device) {
        for (uint8_t i = 0; i < _devices.size(); i++) {
            if (_devices[i] == device) {
                _devices.remove(i);
                return;
            }
        }
    }

    /**
     * @brief Gets all registered devices
     * @return Reference to device array
     */
    DynamicArray<IDevice*>& getDevices() { return _devices; }

    /**
     * @brief Updates all registered devices
     * @details Calls update() on each device. Should be called from main loop.
     */
    void updateAll() {
        for (uint8_t i = 0; i < _devices.size(); i++) {
            _devices[i]->update();
        }
    }
};

#endif