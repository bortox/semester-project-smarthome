/**
 * @file PhysicalInput.h
 * @brief Hardware Abstraction Layer for physical inputs
 * @ingroup HAL
 */
#ifndef PHYSICAL_INPUT_H
#define PHYSICAL_INPUT_H

#include "CoreSystem.h"

// Forward declarations
class SimpleLight;
class DimmableLight;
class NavigationManager;

enum class ButtonMode : uint8_t {
    ACTIVE_LOW,   // Bottone a GND quando premuto (pull-up attivo)
    ACTIVE_HIGH   // Bottone a VCC quando premuto (pull-down esterno)
};

class ButtonInput : public IEventListener {
private:
    uint8_t _pin;
    uint8_t _buttonId;
    bool _lastState;
    unsigned long _lastDebounceTime;
    IDevice* _linkedDevice;
    ButtonMode _mode;
    static const uint8_t DEBOUNCE_DELAY = 50;

public:
    ButtonInput(uint8_t pin, uint8_t buttonId, IDevice* linkedDevice = nullptr, ButtonMode mode = ButtonMode::ACTIVE_LOW);
    
    void update();
    void onButtonPressed();
    void handleEvent(EventType type, IDevice* device, int value) override;
    void setLinkedDevice(IDevice* device) { _linkedDevice = device; }
    IDevice* getLinkedDevice() const { return _linkedDevice; }
};

class PotentiometerInput {
private:
    uint8_t _pin;
    DimmableLight* _light;
    uint8_t _lastMappedValue;
    static constexpr uint8_t SAMPLE_COUNT = 8;
    static constexpr uint8_t POT_OFF_THRESHOLD = 5;
    static constexpr uint8_t POT_CHANGE_THRESHOLD = 3;
    uint16_t _samples[SAMPLE_COUNT];
    uint8_t _sampleIndex;

public:
    PotentiometerInput(uint8_t pin, DimmableLight* linkedLight = nullptr);
    void update();
    void setLinkedLight(DimmableLight* light) { _light = light; }
    DimmableLight* getLinkedLight() const { return _light; }
};

/**
 * @class NavButtonInput
 * @brief Button input specifically for menu navigation
 */
class NavButtonInput {
private:
    uint8_t _pin;
    InputEvent _command;  // Strongly typed event
    bool _lastState;
    unsigned long _lastDebounceTime;
    ButtonMode _mode;
    static const uint8_t DEBOUNCE_DELAY = 50;

public:
    NavButtonInput(uint8_t pin, InputEvent command, ButtonMode mode = ButtonMode::ACTIVE_LOW);
    void update();
};

/**
 * @class InputManager
 * @brief Singleton managing all physical inputs
 */
class InputManager {
private:
    DynamicArray<ButtonInput*> _buttons;
    DynamicArray<PotentiometerInput*> _potentiometers;
    DynamicArray<NavButtonInput*> _navButtons;

    InputManager() {}

public:
    static InputManager& instance();
    void registerButton(ButtonInput* button);
    void registerPotentiometer(PotentiometerInput* pot);
    void registerNavButton(NavButtonInput* navBtn);

    void updateAll();
};

#endif