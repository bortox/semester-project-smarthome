/**
 * @file Scenes.h
 * @brief Scene management and concrete scene implementations
 * @ingroup Automation
 */
#ifndef SCENES_H
#define SCENES_H

#include "CoreSystem.h"
#include "Devices.h"

/**
 * @class IScene
 * @brief Abstract interface for scene implementations
 * @ingroup Automation
 * 
 * Scenes are layers that can modify device states. Multiple scenes can be
 * active simultaneously, with conflicts resolved by priority (Painter's Algorithm).
 * Higher priority scenes are applied last, overwriting lower priority ones.
 */
class IScene {
protected:
    bool _active;
    const char* _name;

public:
    IScene(const char* name) : _active(false), _name(name) {}
    virtual ~IScene() {}

    /**
     * @brief Gets the scene priority (0-255)
     * @return Priority value (higher = applied later = overrides others)
     */
    virtual uint8_t getPriority() const = 0;

    /**
     * @brief Gets the scene name
     * @return Pointer to name string
     */
    const char* getName() const { return _name; }

    /**
     * @brief Checks if scene is currently active
     * @return true if active, false otherwise
     */
    bool isActive() const { return _active; }

    /**
     * @brief Called when scene is activated
     * 
     * Override to perform initialization (e.g., register event listeners,
     * save device states, apply global settings)
     */
    virtual void enter() { _active = true; }

    /**
     * @brief Called when scene is deactivated
     * 
     * Override to perform cleanup (e.g., unregister listeners,
     * restore previous states)
     */
    virtual void exit() { _active = false; }

    /**
     * @brief Periodic update - applies scene effects to devices
     * 
     * Called every frame by SceneManager. Should be non-blocking.
     * Higher priority scenes are called last, allowing them to override.
     */
    virtual void update() = 0;
};

/**
 * @class SceneManager
 * @brief Singleton managing active scenes with priority-based conflict resolution
 * @ingroup Automation
 * 
 * Maintains a list of active scenes sorted by priority. During update(), scenes
 * are applied in ascending priority order (Painter's Algorithm), allowing higher
 * priority scenes to override lower ones. This provides deterministic behavior
 * without complex state merging.
 */
class SceneManager {
private:
    DynamicArray<IScene*> _activeScenes;
    
    SceneManager() {}

public:
    /**
     * @brief Gets the singleton instance
     * @return Reference to SceneManager instance
     */
    static SceneManager& instance() {
        static SceneManager inst;
        return inst;
    }

    /**
     * @brief Activates a scene and adds it to the active list
     * @param scene Scene to activate
     * @return true if added successfully, false if already active or array full
     */
    bool addScene(IScene* scene) {
        if (!scene) return false;
        
        // Check if already active
        for (uint8_t i = 0; i < _activeScenes.size(); i++) {
            if (_activeScenes[i] == scene) return false;
        }
        
        scene->enter();
        return _activeScenes.add(scene);
    }

    /**
     * @brief Deactivates a scene and removes it from the active list
     * @param scene Scene to deactivate
     */
    void removeScene(IScene* scene) {
        if (!scene) return;
        
        for (uint8_t i = 0; i < _activeScenes.size(); i++) {
            if (_activeScenes[i] == scene) {
                scene->exit();
                _activeScenes.remove(i);
                return;
            }
        }
    }

    /**
     * @brief Deactivates all scenes
     */
    void clearAll() {
        for (uint8_t i = 0; i < _activeScenes.size(); i++) {
            _activeScenes[i]->exit();
        }
        _activeScenes.clear();
    }

    /**
     * @brief Periodic update - applies all active scenes in priority order
     * 
     * Implements Painter's Algorithm: sorts scenes by priority (low to high)
     * and calls update() sequentially. This allows high-priority scenes to
     * overwrite low-priority ones deterministically.
     * 
     * Uses bubble sort (acceptable for small arrays, ~5 scenes max).
     */
    void update() {
        uint8_t count = _activeScenes.size();
        if (count == 0) return;
        
        // Bubble sort by priority (ascending)
        for (uint8_t i = 0; i < count - 1; i++) {
            for (uint8_t j = 0; j < count - i - 1; j++) {
                if (_activeScenes[j]->getPriority() > _activeScenes[j + 1]->getPriority()) {
                    // Swap
                    IScene* temp = _activeScenes[j];
                    _activeScenes[j] = _activeScenes[j + 1];
                    _activeScenes[j + 1] = temp;
                }
            }
        }
        
        // Apply scenes in order (lowest priority first)
        for (uint8_t i = 0; i < count; i++) {
            _activeScenes[i]->update();
        }
    }

    /**
     * @brief Gets the number of active scenes
     * @return Count of active scenes
     */
    uint8_t getActiveCount() const {
        return _activeScenes.size();
    }
};

// ==================== CONCRETE SCENES ====================

/**
 * @class NightModeScene
 * @brief Low priority scene that limits global brightness
 * @ingroup Automation
 * 
 * Priority: 10 (Low - applied first, can be overridden by others)
 * 
 * Reduces all light brightness to 20% by modifying the global multiplier.
 * This is a passive effect applied via DimmableLight::setBrightnessMultiplier(),
 * so update() is empty.
 */
class NightModeScene : public IScene {
private:
    uint8_t _savedMultiplier;

public:
    NightModeScene() : IScene("Night Mode"), _savedMultiplier(100) {}

    uint8_t getPriority() const override { return 10; }

    void enter() override {
        IScene::enter();
        // Save current multiplier and apply night mode
        _savedMultiplier = 100; // Assume default is 100
        DimmableLight::setBrightnessMultiplier(20);
    }

    void exit() override {
        // Restore previous brightness multiplier
        DimmableLight::setBrightnessMultiplier(_savedMultiplier);
        IScene::exit();
    }

    void update() override {
        // Passive effect via multiplier - no per-frame logic needed
    }
};

/**
 * @class PartyScene
 * @brief Medium priority scene that cycles RGB light colors
 * @ingroup Automation
 * 
 * Priority: 50 (Medium - can be overridden by Alarm, overrides Night Mode)
 * 
 * Cycles all RGBLight devices through Red -> Green -> Blue -> repeat
 * every 500ms using non-blocking timing.
 */
class PartyScene : public IScene {
private:
    unsigned long _lastChange;
    uint8_t _colorIndex; // 0=Red, 1=Green, 2=Blue
    static constexpr unsigned long CHANGE_INTERVAL_MS = 500;

public:
    PartyScene() : IScene("Party Mode"), _lastChange(0), _colorIndex(0) {}

    uint8_t getPriority() const override { return 50; }

    void enter() override {
        IScene::enter();
        _lastChange = millis();
        _colorIndex = 0;
    }

    void update() override {
        if (!_active) return;
        
        unsigned long now = millis();
        if (now - _lastChange >= CHANGE_INTERVAL_MS) {
            _lastChange = now;
            
            // Cycle color
            _colorIndex = (_colorIndex + 1) % 3;
            
            // Apply to all RGB lights
            DeviceRegistry& registry = DeviceRegistry::instance();
            DynamicArray<IDevice*>& devices = registry.getDevices();
            
            for (uint8_t i = 0; i < devices.size(); i++) {
                if (devices[i]->type == DeviceType::LightRGB) {
                    RGBLight* rgb = static_cast<RGBLight*>(devices[i]);
                    
                    // Set color based on index
                    RGBColor color;
                    switch (_colorIndex) {
                        case 0: color = {255, 0, 0}; break;   // Red
                        case 1: color = {0, 255, 0}; break;   // Green
                        case 2: color = {0, 0, 255}; break;   // Blue
                    }
                    rgb->setColor(color);
                    
                    // Ensure light is on
                    if (rgb->getBrightness() == 0) {
                        rgb->toggle();
                    }
                }
            }
        }
    }
};

/**
 * @class AlarmScene
 * @brief Highest priority scene that flashes red on motion detection
 * @ingroup Automation
 * 
 * Priority: 255 (Maximum - overrides all other scenes)
 * 
 * Listens to PIR motion sensor events. When motion is detected, flashes
 * all RGBLight devices red (ON/OFF) every 200ms. If no motion for 10 seconds,
 * stops flashing and becomes transparent (allows underlying scenes to show).
 */
class AlarmScene : public IScene, public IEventListener {
private:
    bool _triggered;
    bool _flashState;
    unsigned long _lastFlash;
    unsigned long _lastMotion;
    static constexpr unsigned long FLASH_INTERVAL_MS = 200;
    static constexpr unsigned long TIMEOUT_MS = 10000; // 10 seconds

public:
    AlarmScene() : IScene("Alarm Mode"), _triggered(false), 
                   _flashState(false), _lastFlash(0), _lastMotion(0) {}

    uint8_t getPriority() const override { return 255; }

    void enter() override {
        IScene::enter();
        _triggered = false;
        _flashState = false;
        _lastFlash = millis();
        _lastMotion = millis();
        
        // Subscribe to sensor events
        EventSystem::instance().addListener(this, EventType::SensorUpdated);
    }

    void exit() override {
        EventSystem::instance().removeListener(this);
        
        // Restore lights to OFF state
        DeviceRegistry& registry = DeviceRegistry::instance();
        DynamicArray<IDevice*>& devices = registry.getDevices();
        
        for (uint8_t i = 0; i < devices.size(); i++) {
            if (devices[i]->type == DeviceType::LightRGB) {
                RGBLight* rgb = static_cast<RGBLight*>(devices[i]);
                rgb->setBrightness(0);
            }
        }
        
        IScene::exit();
    }

    void handleEvent(EventType type, IDevice* source, int value) override {
        if (type == EventType::SensorUpdated && source->type == DeviceType::SensorPIR) {
            if (value == 1) { // Motion detected
                _triggered = true;
                _lastMotion = millis();
            }
        }
    }

    void update() override {
        if (!_active) return;
        
        unsigned long now = millis();
        
        // Check timeout
        if (_triggered && (now - _lastMotion > TIMEOUT_MS)) {
            _triggered = false;
            _flashState = false;
            
            // Turn off all RGB lights
            DeviceRegistry& registry = DeviceRegistry::instance();
            DynamicArray<IDevice*>& devices = registry.getDevices();
            
            for (uint8_t i = 0; i < devices.size(); i++) {
                if (devices[i]->type == DeviceType::LightRGB) {
                    RGBLight* rgb = static_cast<RGBLight*>(devices[i]);
                    rgb->setBrightness(0);
                }
            }
        }
        
        // Flash logic
        if (_triggered && (now - _lastFlash >= FLASH_INTERVAL_MS)) {
            _lastFlash = now;
            _flashState = !_flashState;
            
            // Apply flash to all RGB lights
            DeviceRegistry& registry = DeviceRegistry::instance();
            DynamicArray<IDevice*>& devices = registry.getDevices();
            
            RGBColor red = {255, 0, 0};
            
            for (uint8_t i = 0; i < devices.size(); i++) {
                if (devices[i]->type == DeviceType::LightRGB) {
                    RGBLight* rgb = static_cast<RGBLight*>(devices[i]);
                    rgb->setColor(red);
                    rgb->setBrightness(_flashState ? 100 : 0);
                }
            }
        }
    }
};

#endif
