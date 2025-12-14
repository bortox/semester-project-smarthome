/**
 * @file CoreSystem.h
 * @brief Core system definitions, interfaces, and memory management
 * @ingroup Core
 */
#ifndef CORE_SYSTEM_H
#define CORE_SYSTEM_H

#include <Arduino.h>

// --- Enums ---
/**
 * @enum DeviceType
 * @brief Enumeration of supported device types
 * @ingroup Core
 */
enum class DeviceType : uint8_t {
    LightSimple,        ///< Basic On/Off light
    LightDimmable,      ///< Light with brightness control
    LightRGB,           ///< RGB Light with color control
    LightOutside,       ///< Outdoor light with automation logic
    SensorTemperature,  ///< Temperature sensor (LM75)
    SensorLight,        ///< Photoresistor light sensor
    SensorPIR,          ///< Passive Infrared motion sensor
    Unknown             ///< Undefined device type
};

/**
 * @enum EventType
 * @brief Types of events propagated through the system
 * @ingroup Core
 */
enum EventType : uint8_t {  // uint8_t instead of int
    EVENT_NONE,             ///< No event
    DeviceStateChanged,     ///< Device turned On/Off
    DeviceValueChanged,     ///< Device value (brightness/color) changed
    SensorUpdated,          ///< Sensor reading updated
    ButtonPressed,          ///< Physical button pressed
    EVENT_ALARM             ///< Alarm system triggered
};

/**
 * @enum InputEvent
 * @brief Navigation events for the menu system
 * @ingroup Core
 */
enum class InputEvent : uint8_t {
    NONE,
    UP,
    DOWN,
    ENTER,
    BACK
};

// --- DynamicArray ottimizzato ---
/**
 * @class DynamicArray
 * @brief Memory-optimized dynamic array implementation
 * @tparam T Type of elements stored in the array
 * @ingroup Core
 * 
 * A lightweight vector-like container optimized for embedded systems.
 * 
 * @warning Not interrupt-safe.
 * @warning Uses manual memory management (new/delete). Ensure heap space is sufficient.
 * 
 * Growth Strategy:
 * - Initial capacity: 0
 * - Growth step: 4 elements
 * - Max capacity: 64 elements
 * - Shrink threshold: 8 unused slots
 */
template <typename T>
class DynamicArray {
private:
    T* _data;
    uint8_t _size;
    uint8_t _capacity;
    
    static constexpr uint8_t GROW_STEP = 4; 
    static constexpr uint8_t MAX_CAPACITY = 64;
    static constexpr uint8_t SHRINK_THRESHOLD = 8; // Shrink when unused space > 8

public:
    DynamicArray() : _data(nullptr), _size(0), _capacity(0) {}
    
    ~DynamicArray() {
        delete[] _data;
    }

    // Memory Safety: Prevent accidental copies (saves RAM, prevents double-free)
    DynamicArray(const DynamicArray&) = delete;
    DynamicArray& operator=(const DynamicArray&) = delete;

    /**
     * @brief Adds an element to the array
     * @param item Element to add
     * @return true if successful, false if capacity limit reached or allocation failed
     */
    bool add(const T& item) {
        if (_size >= _capacity) {
            uint8_t newCapacity = _capacity + GROW_STEP;
            
            if (newCapacity > MAX_CAPACITY) {
                if (_capacity < MAX_CAPACITY) newCapacity = MAX_CAPACITY;
                else return false; 
            }

            T* newData = new T[newCapacity];
            if (!newData) return false; 

            for (uint8_t i = 0; i < _size; i++) {
                newData[i] = _data[i];
            }

            delete[] _data;
            _data = newData;
            _capacity = newCapacity;
        }

        _data[_size++] = item;
        return true;
    }

    /**
     * @brief Removes an element at specific index
     * @param index Index of element to remove
     * 
     * Shifts subsequent elements left. Triggers shrink if waste exceeds threshold.
     */
    void remove(uint8_t index) {
        if (index < _size) {
            for (uint8_t i = index; i < _size - 1; i++) {
                _data[i] = _data[i + 1];
            }
            _size--;
            
            // NEW: Auto-shrink to save RAM when waste is significant
            if (_capacity > SHRINK_THRESHOLD && (_capacity - _size) >= SHRINK_THRESHOLD) {
                shrink();
            }
        }
    }

    // NEW: Manual memory reclamation
    void shrink() {
        if (_size == 0) {
            delete[] _data;
            _data = nullptr;
            _capacity = 0;
            return;
        }
        
        uint8_t newCapacity = ((_size / GROW_STEP) + 1) * GROW_STEP;
        if (newCapacity >= _capacity) return;
        
        T* newData = new T[newCapacity];
        if (!newData) return;
        
        for (uint8_t i = 0; i < _size; i++) {
            newData[i] = _data[i];
        }
        
        delete[] _data;
        _data = newData;
        _capacity = newCapacity;
    }

    T& operator[](uint8_t index) { return _data[index]; }
    const T& operator[](uint8_t index) const { return _data[index]; }
    uint8_t size() const { return _size; }
    void clear() { _size = 0; } // Nota: non libera memoria, resetta solo counter
};

// --- Interfaces ---
/**
 * @class IDevice
 * @brief Abstract base interface for all hardware devices
 * @ingroup Devices
 */
class IDevice {
public:
    const char* const name;
    const DeviceType type;
    
    IDevice(const char* n, DeviceType t) : name(n), type(t) {}
    virtual ~IDevice() {}
    virtual void update() = 0;
    virtual bool isLight() const { return false; }
    virtual bool isSensor() const { return false; }
};

// Forward declaration
class MenuPage;

// Tipi per JIT Menu e Slider
typedef MenuPage* (*PageBuilder)(void* context);
typedef int (*ValueGetter)(IDevice*);
typedef void (*ValueSetter)(IDevice*, int);

/**
 * @class IEventListener
 * @brief Interface for the Observer Pattern
 * @ingroup Core
 * 
 * Classes implementing this interface can subscribe to system events.
 */
class IEventListener {
public:
    virtual ~IEventListener() {}
    /**
     * @brief Callback triggered when an event occurs
     * @param type Type of event
     * @param source Pointer to the device that originated the event
     * @param value Optional data value associated with the event
     */
    virtual void handleEvent(EventType type, IDevice* source, int value) = 0;
};

// --- Event System ---
/**
 * @class EventSystem
 * @brief Singleton managing event propagation (Observer Pattern)
 * @ingroup Core
 * 
 * Central hub for dispatching events from producers (Inputs/Sensors) 
 * to consumers (UI/Logic).
 */
class EventSystem {
private:
    struct ListenerEntry {
        IEventListener* listener;
        uint8_t eventMask;  // Bitmask per eventi (max 8 tipi)
    };
    
    DynamicArray<ListenerEntry> _listeners;
    EventSystem() {}

public:
    /**
     * @brief Access the singleton instance
     * @return Reference to EventSystem
     */
    static EventSystem& instance() {
        static EventSystem inst;
        return inst;
    }

    void addListener(IEventListener* listener) {
        // Ascolta tutti gli eventi (mask = 0xFF)
        ListenerEntry entry = {listener, 0xFF};
        _listeners.add(entry);
    }

    void addListener(IEventListener* listener, EventType type) {
        // FIX: Cerca se listener già registrato, altrimenti crea nuovo
        for (uint8_t i = 0; i < _listeners.size(); i++) {
            if (_listeners[i].listener == listener) {
                // Aggiungi tipo alla mask esistente
                _listeners[i].eventMask |= (1 << type);
                return;
            }
        }
        
        // Nuovo listener, crea entry con solo questo tipo
        ListenerEntry entry = {listener, (uint8_t)(1 << type)};
        _listeners.add(entry);
    }

    void removeListener(IEventListener* listener) {
        for (uint8_t i = 0; i < _listeners.size(); i++) {
            if (_listeners[i].listener == listener) {
                _listeners.remove(i);
                return;
            }
        }
    }

    void emit(EventType type, IDevice* source, int value) {
        // FIX: Filtra listener per tipo evento
        for (uint8_t i = 0; i < _listeners.size(); i++) {
            if (_listeners[i].eventMask & (1 << type)) {
                _listeners[i].listener->handleEvent(type, source, value);
            }
        }
    }
};

// --- Device Registry ---
/**
 * @class DeviceRegistry
 * @brief Singleton registry for all system devices
 * @ingroup Core
 * 
 * Acts as a central repository for accessing devices by index or type.
 */
class DeviceRegistry {
private:
    DynamicArray<IDevice*> _devices;
    DeviceRegistry() {}

public:
    static DeviceRegistry& instance() {
        static DeviceRegistry inst;
        return inst;
    }

    void registerDevice(IDevice* device) {
        _devices.add(device);
    }

    void unregisterDevice(IDevice* device) {
        for (uint8_t i = 0; i < _devices.size(); i++) {
            if (_devices[i] == device) {
                _devices.remove(i);
                return;
            }
        }
    }

    DynamicArray<IDevice*>& getDevices() {
        return _devices;
    }
};

#endif