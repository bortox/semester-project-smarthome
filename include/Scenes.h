/**
 * @file Scenes.h
 * @brief Scene management system for automated lighting control
 * @author Andrea Bortolotti
 * @version 2.0
 * @ingroup Automation
 * 
 * Implements a multi-layer scene architecture with priority-based resolution.
 * Multiple scenes can be active simultaneously, with higher priority scenes
 * overriding lower priority ones (Painter's Algorithm).
 */
#ifndef SCENES_H
#define SCENES_H

#include <Arduino.h>
#include "CoreSystem.h"

class DimmableLight;
class RGBLight;
class PIRSensorDevice;

/**
 * @class IScene
 * @brief Abstract base class for automation scenes
 * @ingroup Automation
 * 
 * Scenes modify device states based on conditions or timers.
 * Each scene has a priority level for conflict resolution.
 * Higher priority scenes are applied last, overwriting lower priority ones.
 */
class IScene {
protected:
    bool _active;           ///< Current activation state
    uint8_t _priority;      ///< Scene priority (0-255, higher = applied later)

public:
    /**
     * @brief Constructs scene with priority
     * @param priority Scene priority (higher = applied later = overrides)
     */
    IScene(uint8_t priority);
    
    /**
     * @brief Virtual destructor for proper cleanup
     */
    virtual ~IScene() {}

    /**
     * @brief Periodic update - applies scene effects to devices
     * @details Called every frame by SceneManager. Should be non-blocking.
     */
    virtual void update() = 0;
    
    /**
     * @brief Called when scene is activated
     * @details Override to perform initialization (e.g., register listeners, apply settings)
     */
    virtual void onActivate() = 0;
    
    /**
     * @brief Called when scene is deactivated
     * @details Override to perform cleanup (e.g., unregister listeners, restore states)
     */
    virtual void onDeactivate() = 0;

    /**
     * @brief Gets scene display name
     * @return Null-terminated string stored in PROGMEM
     */
    virtual const char* getName() const = 0;

    /**
     * @brief Checks if scene is active
     * @return True if active
     */
    bool isActive() const;
    
    /**
     * @brief Gets scene priority
     * @return Priority value (0-255)
     */
    uint8_t getPriority() const;
    
    /**
     * @brief Sets scene active state and calls appropriate callback
     * @param active New state
     */
    void setActive(bool active);
};

/**
 * @class SceneManager
 * @brief Singleton manager for scene lifecycle and execution
 * @ingroup Automation
 * 
 * Maintains sorted list of active scenes by priority.
 * Updates all active scenes each loop iteration using Painter's Algorithm.
 */
class SceneManager {
private:
    DynamicArray<IScene*> _activeScenes;  ///< Active scenes sorted by priority

    /**
     * @brief Private constructor for singleton pattern
     */
    SceneManager() {}

public:
    /**
     * @brief Gets singleton instance
     * @return Reference to SceneManager
     */
    static SceneManager& instance();

    /**
     * @brief Activates a scene and adds it to the active list
     * @param scene Scene to add (inserted by priority)
     * @return True if added successfully
     */
    bool addScene(IScene* scene);
    
    /**
     * @brief Deactivates a scene and removes it from the active list
     * @param scene Scene to remove
     */
    void removeScene(IScene* scene);
    
    /**
     * @brief Deactivates all active scenes
     */
    void clearAll();
    
    /**
     * @brief Updates all active scenes in priority order
     * @details Implements Painter's Algorithm - low priority first
     */
    void update();
    
    /**
     * @brief Gets count of active scenes
     * @return Number of active scenes
     */
    uint8_t getActiveCount() const;
};

/**
 * @class NightModeScene
 * @brief Night mode scene - reduces all light brightness
 * @ingroup Automation
 * 
 * Priority: 10 (low - base layer)
 * Effect: Sets global brightness multiplier to 20%
 */
class NightModeScene : public IScene {
private:
    uint8_t _savedMultiplier;  ///< Saved multiplier for restoration

public:
    /**
     * @brief Constructs night mode scene
     */
    NightModeScene();
    
    void update() override;
    void onActivate() override;
    void onDeactivate() override;
    const char* getName() const override;
};

/**
 * @class PartyScene
 * @brief Party mode scene - RGB color cycling
 * @ingroup Automation
 * 
 * Priority: 50 (medium)
 * Effect: Cycles RGB lights through Red -> Green -> Blue every 500ms
 */
class PartyScene : public IScene {
private:
    unsigned long _lastChange;  ///< Timestamp of last color change
    uint8_t _colorIndex;        ///< Current color index (0=R, 1=G, 2=B)
    static constexpr unsigned long CHANGE_INTERVAL_MS = 500;  ///< Color change interval

public:
    /**
     * @brief Constructs party mode scene
     */
    PartyScene();
    
    void update() override;
    void onActivate() override;
    void onDeactivate() override;
    const char* getName() const override;
};

/**
 * @class AlarmScene
 * @brief Alarm mode scene - motion-triggered red flash
 * @ingroup Automation
 * 
 * Priority: 255 (highest - emergency override)
 * Effect: Flashes all RGB lights red when PIR detects motion.
 * Auto-deactivates flash after 10 seconds of no motion.
 */
class AlarmScene : public IScene, public IEventListener {
private:
    bool _triggered;            ///< Motion detection triggered flag
    bool _flashState;           ///< Current flash on/off state
    unsigned long _lastFlash;   ///< Timestamp of last flash toggle
    unsigned long _lastMotion;  ///< Timestamp of last motion detection
    static constexpr unsigned long FLASH_INTERVAL_MS = 200;   ///< Flash toggle interval
    static constexpr unsigned long TIMEOUT_MS = 10000;        ///< Motion timeout (10s)

public:
    /**
     * @brief Constructs alarm mode scene
     */
    AlarmScene();
    
    void update() override;
    void onActivate() override;
    void onDeactivate() override;
    const char* getName() const override;
    
    /**
     * @brief Handles sensor events for motion detection
     * @param type Event type
     * @param device Source device
     * @param value Event value (1 = motion detected)
     */
    void handleEvent(EventType type, IDevice* device, int value) override;
};

#endif
