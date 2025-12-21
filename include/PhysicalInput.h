/**
 * @file PhysicalInput.h
 * @brief Hardware Abstraction Layer for physical inputs
 * @author Andrea Bortolotti
 * @version 2.0
 * 
 * @details Provides abstraction for physical input devices:
 * - Push buttons with debouncing
 * - Potentiometers with smoothing
 * - Navigation buttons for menu control
 * 
 * @ingroup HAL
 */
#ifndef PHYSICAL_INPUT_H
#define PHYSICAL_INPUT_H

#include "CoreSystem.h"

class SimpleLight;
class DimmableLight;
class NavigationManager;

/**
 * @brief Button wiring configuration
 * @ingroup HAL
 */
enum class ButtonMode : uint8_t {
    ACTIVE_LOW,   ///< Button connects to GND when pressed (internal pull-up)
    ACTIVE_HIGH   ///< Button connects to VCC when pressed (external pull-down)
};

/**
 * @class ButtonInput
 * @brief Debounced button input handler with device linking
 * @ingroup HAL
 * 
 * @details Handles physical button presses with software debouncing.
 * Can be linked to a device to emit ButtonPressed events when pressed.
 * Implements IEventListener to react to linked device state changes.
 */
class ButtonInput : public IEventListener {
private:
    uint8_t _pin;                    ///< Arduino pin number
    uint8_t _buttonId;               ///< Unique button identifier
    bool _lastState;                 ///< Previous debounced state
    unsigned long _lastDebounceTime; ///< Timestamp of last state change
    IDevice* _linkedDevice;          ///< Device controlled by this button
    ButtonMode _mode;                ///< Wiring configuration
    static const uint8_t DEBOUNCE_DELAY = 50;  ///< Debounce time in milliseconds

public:
    /**
     * @brief Constructor
     * @param pin Arduino pin number
     * @param buttonId Unique identifier for this button
     * @param linkedDevice Device to control (optional)
     * @param mode Button wiring configuration
     */
    ButtonInput(uint8_t pin, uint8_t buttonId, IDevice* linkedDevice = nullptr, 
                ButtonMode mode = ButtonMode::ACTIVE_LOW);
    
    /**
     * @brief Periodic update - reads and debounces button state
     * @note Call from main loop
     */
    void update();
    
    /**
     * @brief Called when valid button press is detected
     * @details Emits ButtonPressed event to linked device
     */
    void onButtonPressed();
    
    /**
     * @brief Event handler for linked device state changes
     * @param type Event type
     * @param device Source device
     * @param value Event value
     */
    void handleEvent(EventType type, IDevice* device, int value) override;
    
    /**
     * @brief Links button to a device
     * @param device Device to control
     */
    void setLinkedDevice(IDevice* device) { _linkedDevice = device; }
    
    /**
     * @brief Gets linked device
     * @return Pointer to linked device or nullptr
     */
    IDevice* getLinkedDevice() const { return _linkedDevice; }
};

/**
 * @class PotentiometerInput
 * @brief Smoothed potentiometer input with light control
 * @ingroup HAL
 * 
 * @details Reads analog potentiometer with averaging and threshold
 * filtering to control dimmable light brightness smoothly.
 */
class PotentiometerInput {
private:
    uint8_t _pin;                    ///< Arduino analog pin number
    DimmableLight* _light;           ///< Linked dimmable light
    uint8_t _lastMappedValue;        ///< Last applied brightness value
    static constexpr uint8_t SAMPLE_COUNT = 8;        ///< Averaging sample count
    static constexpr uint8_t POT_OFF_THRESHOLD = 5;   ///< Threshold below which light turns off
    static constexpr uint8_t POT_CHANGE_THRESHOLD = 3; ///< Minimum change to update
    uint16_t _samples[SAMPLE_COUNT]; ///< Circular buffer for averaging
    uint8_t _sampleIndex;            ///< Current position in sample buffer

public:
    /**
     * @brief Constructor
     * @param pin Arduino analog pin number
     * @param linkedLight Dimmable light to control (optional)
     */
    explicit PotentiometerInput(uint8_t pin, DimmableLight* linkedLight = nullptr);
    
    /**
     * @brief Periodic update - reads and applies potentiometer value
     * @note Call from main loop. Non-blocking.
     */
    void update();
    
    /**
     * @brief Links potentiometer to a light
     * @param light Dimmable light to control
     */
    void setLinkedLight(DimmableLight* light) { _light = light; }
    
    /**
     * @brief Gets linked light
     * @return Pointer to linked light or nullptr
     */
    DimmableLight* getLinkedLight() const { return _light; }
};

/**
 * @class NavButtonInput
 * @brief Navigation button for menu control
 * @ingroup HAL
 * 
 * @details Dedicated button input for menu navigation.
 * Sends strongly-typed InputEvent commands to NavigationManager.
 */
class NavButtonInput {
private:
    uint8_t _pin;                    ///< Arduino pin number
    InputEvent _command;             ///< Navigation command to send
    bool _lastState;                 ///< Previous debounced state
    unsigned long _lastDebounceTime; ///< Timestamp of last state change
    ButtonMode _mode;                ///< Wiring configuration
    static const uint8_t DEBOUNCE_DELAY = 50;  ///< Debounce time in milliseconds

public:
    /**
     * @brief Constructor
     * @param pin Arduino pin number
     * @param command Navigation command to send when pressed
     * @param mode Button wiring configuration
     */
    NavButtonInput(uint8_t pin, InputEvent command, ButtonMode mode = ButtonMode::ACTIVE_LOW);
    
    /**
     * @brief Periodic update - reads and processes button state
     * @note Call from main loop
     */
    void update();
};

/**
 * @class InputManager
 * @brief Singleton managing all physical inputs
 * @ingroup HAL
 * 
 * @details Centralized manager for all button and potentiometer inputs.
 * Provides single update point for all input processing.
 */
class InputManager {
private:
    DynamicArray<ButtonInput*> _buttons;           ///< Registered button inputs
    DynamicArray<PotentiometerInput*> _potentiometers; ///< Registered potentiometer inputs
    DynamicArray<NavButtonInput*> _navButtons;     ///< Registered navigation buttons

    /**
     * @brief Private constructor for singleton pattern
     */
    InputManager() {}

public:
    /**
     * @brief Gets singleton instance
     * @return Reference to InputManager instance
     */
    static InputManager& instance();
    
    /**
     * @brief Registers a button input
     * @param button Pointer to button input
     */
    void registerButton(ButtonInput* button);
    
    /**
     * @brief Registers a potentiometer input
     * @param pot Pointer to potentiometer input
     */
    void registerPotentiometer(PotentiometerInput* pot);
    
    /**
     * @brief Registers a navigation button
     * @param navBtn Pointer to navigation button
     */
    void registerNavButton(NavButtonInput* navBtn);

    /**
     * @brief Updates all registered inputs
     * @note Call from main loop. Non-blocking.
     */
    void updateAll();
};

#endif