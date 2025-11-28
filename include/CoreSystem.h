#ifndef CORE_SYSTEM_H
#define CORE_SYSTEM_H

#include <Arduino.h>

// --- Enums ---
enum class DeviceType : uint8_t {
    LightSimple,
    LightDimmable,
    LightRGB,        // FIX: Aggiunto nuovo tipo
    LightOutside,    // FIX: Aggiunto nuovo tipo
    SensorTemperature,
    SensorLight,    // FIX: Nuovo tipo
    Unknown
};

enum EventType : uint8_t {  // uint8_t invece di int
    EVENT_NONE,
    DeviceStateChanged,
    DeviceValueChanged,
    SensorUpdated,
    ButtonPressed,
    EVENT_ALARM
};

// --- DynamicArray ottimizzato ---
template <typename T>
class DynamicArray {
private:
    T* _data;
    uint8_t _size;      // uint8_t invece di size_t (max 255 items)
    uint8_t _capacity;

public:
    DynamicArray() : _data(nullptr), _size(0), _capacity(0) {}
    
    ~DynamicArray() {
        delete[] _data;
    }

    bool add(T item) {
        if (_size == _capacity) {
            // OTTIMIZZAZIONE: Crescita lineare +2 invece di esponenziale ×2
            uint8_t newCapacity = (_capacity == 0) ? 2 : _capacity + 2;
            if (newCapacity > 16) return false; // Limite max 16 items
            
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

    void remove(uint8_t index) {
        if (index < _size) {
            for (uint8_t i = index; i < _size - 1; i++) {
                _data[i] = _data[i + 1];
            }
            _size--;
        }
    }

    T& operator[](uint8_t index) { return _data[index]; }
    const T& operator[](uint8_t index) const { return _data[index]; }
    uint8_t size() const { return _size; }
    void clear() { _size = 0; }
};

// --- Interfaces ---
class IDevice {
public:
    virtual ~IDevice() {}
    virtual const char* getName() const = 0;
    virtual void update() = 0;
    virtual DeviceType getType() const = 0;
    virtual bool isLight() const { return false; }
    virtual bool isSensor() const { return false; }
};

// Forward declaration
class MenuPage;

// Tipi per JIT Menu e Slider
typedef MenuPage* (*PageBuilder)(void* context);
typedef int (*ValueGetter)(IDevice*);
typedef void (*ValueSetter)(IDevice*, int);

class IEventListener {
public:
    virtual ~IEventListener() {}
    virtual void handleEvent(EventType type, IDevice* source, int value) = 0;
};

// --- Event System ---
class EventSystem {
private:
    struct ListenerEntry {
        IEventListener* listener;
        uint8_t eventMask;  // Bitmask per eventi (max 8 tipi)
    };
    
    DynamicArray<ListenerEntry> _listeners;
    EventSystem() {}

public:
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
    
    uint8_t getDeviceCount() const {
        return _devices.size();
    }
};

#endif