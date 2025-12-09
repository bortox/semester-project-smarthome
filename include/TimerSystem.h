/**
 * @file TimerSystem.h
 * @brief Scheduling system for delayed actions
 * @ingroup Automation
 */
#ifndef TIMER_SYSTEM_H
#define TIMER_SYSTEM_H

#include "CoreSystem.h"

// Forward declarations
class IScene;
class SimpleLight;
class DimmableLight;

/**
 * @struct TimerTask
 * @brief Represents a scheduled action to be executed at a future time
 * @ingroup Automation
 * 
 * Supports both device actions (TURN_ON, TOGGLE, etc.) and scene actions
 * (ACTIVATE_SCENE, DEACTIVATE_SCENE). Uses union to save RAM by storing
 * either a device pointer or scene pointer, never both simultaneously.
 */
struct TimerTask {
    unsigned long expireTime;   ///< Absolute time (millis) when task executes
    TimerAction action;          ///< Action to perform
    uint8_t value;               ///< Optional parameter (e.g., brightness 0-100)
    
    union {
        IDevice* device;         ///< Target device (for device actions)
        IScene* scene;           ///< Target scene (for scene actions)
    };
    
    /**
     * @brief Default constructor (required by DynamicArray)
     * 
     * Initializes task with safe default values. Used internally
     * by DynamicArray when resizing the internal storage.
     */
    TimerTask() 
        : expireTime(0), action(TimerAction::TURN_OFF), value(0), device(nullptr) {}
    
    /**
     * @brief Constructor for device-based timer
     * @param time Expiration time in milliseconds
     * @param act Action to perform
     * @param dev Target device
     * @param val Optional value parameter
     */
    TimerTask(unsigned long time, TimerAction act, IDevice* dev, uint8_t val = 0)
        : expireTime(time), action(act), value(val), device(dev) {}
    
    /**
     * @brief Constructor for scene-based timer
     * @param time Expiration time in milliseconds
     * @param act Action to perform (must be scene-related)
     * @param scn Target scene
     */
    TimerTask(unsigned long time, TimerAction act, IScene* scn)
        : expireTime(time), action(act), value(0), scene(scn) {}
};

/**
 * @class TimerManager
 * @brief Singleton managing scheduled task execution without heap-heavy callbacks
 * @ingroup Automation
 * 
 * Uses an enum-based approach to avoid std::function overhead. Each TimerTask
 * contains all necessary data, and actions are resolved via switch statement
 * at execution time. Tasks are automatically removed after execution.
 * 
 * Memory footprint: ~8 bytes per task + DynamicArray overhead
 */
class TimerManager {
private:
    DynamicArray<TimerTask> _tasks;
    
    TimerManager() {}

public:
    /**
     * @brief Gets the singleton instance
     * @return Reference to TimerManager instance
     */
    static TimerManager& instance() {
        static TimerManager inst;
        return inst;
    }

    /**
     * @brief Schedules a device action after a delay
     * @param delayMs Delay in milliseconds from now
     * @param action Action to perform on device
     * @param device Target device pointer
     * @param value Optional parameter (e.g., brightness level)
     * @return true if task was scheduled, false if array is full
     */
    bool addTimer(unsigned long delayMs, TimerAction action, IDevice* device, uint8_t value = 0) {
        if (!device) return false;
        
        unsigned long expireTime = millis() + delayMs;
        TimerTask task(expireTime, action, device, value);
        return _tasks.add(task);
    }

    /**
     * @brief Schedules a scene action after a delay
     * @param delayMs Delay in milliseconds from now
     * @param action Action to perform (ACTIVATE_SCENE or DEACTIVATE_SCENE)
     * @param scene Target scene pointer
     * @return true if task was scheduled, false if array is full
     */
    bool addSceneTimer(unsigned long delayMs, TimerAction action, IScene* scene) {
        if (!scene) return false;
        if (action != TimerAction::ACTIVATE_SCENE && action != TimerAction::DEACTIVATE_SCENE) {
            return false;
        }
        
        unsigned long expireTime = millis() + delayMs;
        TimerTask task(expireTime, action, scene);
        return _tasks.add(task);
    }

    /**
     * @brief Periodic update - executes expired tasks
     * 
     * Iterates through all pending tasks, executes those whose expireTime
     * has passed, and removes them. Uses reverse iteration to safely remove
     * during traversal. Execution is done via switch statement on action type.
     */
    void update() {
        unsigned long now = millis();
        
        // Reverse iterate to safely remove during traversal
        for (uint8_t i = _tasks.size(); i > 0; i--) {
            uint8_t idx = i - 1;
            TimerTask& task = _tasks[idx];
            
            if (now >= task.expireTime) {
                executeTask(task);
                _tasks.remove(idx);
            }
        }
    }

    /**
     * @brief Clears all pending timers
     */
    void clearAll() {
        _tasks.clear();
    }

    /**
     * @brief Gets the number of pending timers
     * @return Number of scheduled tasks
     */
    uint8_t getPendingCount() const {
        return _tasks.size();
    }

private:
    /**
     * @brief Executes a timer task based on its action type
     * @param task Task to execute
     * 
     * Uses switch statement to avoid function pointer overhead.
     * Performs null checks before casting and calling device methods.
     */
    void executeTask(TimerTask& task) {
        switch (task.action) {
            case TimerAction::TURN_ON:
                if (task.device && task.device->isLight()) {
                    SimpleLight* light = static_cast<SimpleLight*>(task.device);
                    if (!light->getState()) {
                        light->toggle();
                    }
                }
                break;

            case TimerAction::TURN_OFF:
                if (task.device && task.device->isLight()) {
                    SimpleLight* light = static_cast<SimpleLight*>(task.device);
                    if (light->getState()) {
                        light->toggle();
                    }
                }
                break;

            case TimerAction::TOGGLE:
                if (task.device && task.device->isLight()) {
                    SimpleLight* light = static_cast<SimpleLight*>(task.device);
                    light->toggle();
                }
                break;

            case TimerAction::SET_BRIGHTNESS:
                if (task.device && task.device->type == DeviceType::LightDimmable) {
                    DimmableLight* dimLight = static_cast<DimmableLight*>(task.device);
                    dimLight->setBrightness(task.value);
                } else if (task.device && task.device->type == DeviceType::LightRGB) {
                    DimmableLight* dimLight = static_cast<DimmableLight*>(task.device);
                    dimLight->setBrightness(task.value);
                }
                break;

            case TimerAction::ACTIVATE_SCENE:
            case TimerAction::DEACTIVATE_SCENE:
                // Handled by SceneManager via TimerTask callback
                // Scene pointer stored in task.scene
                break;
        }
    }
};

#endif
