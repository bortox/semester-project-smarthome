#ifndef DEVICES_H
#define DEVICES_H

#include "CoreSystem.h"
#include "sensors.h"
#include <avr/pgmspace.h>

#define PWM_MIN 0
#define PWM_MAX 255

/**
 * @brief Gamma correction lookup table (256 entries)
 * 
 * Safe low-end curve: input 1 maps to output 1 (not 0) to avoid black crush.
 * Generated using gamma = 2.2
 */
const uint8_t GAMMA_LUT[256] PROGMEM = {
    0,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
    2,   2,   2,   2,   2,   2,   2,   2,   3,   3,   3,   3,   3,   3,   4,   4,
    4,   4,   4,   5,   5,   5,   5,   6,   6,   6,   6,   7,   7,   7,   8,   8,
    8,   9,   9,   9,   10,  10,  11,  11,  11,  12,  12,  13,  13,  14,  14,  15,
    15,  16,  16,  17,  17,  18,  18,  19,  19,  20,  20,  21,  22,  22,  23,  23,
    24,  25,  25,  26,  26,  27,  28,  28,  29,  30,  30,  31,  32,  33,  33,  34,
    35,  35,  36,  37,  38,  39,  39,  40,  41,  42,  43,  43,  44,  45,  46,  47,
    48,  49,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  62,
    63,  64,  65,  66,  67,  68,  69,  70,  71,  73,  74,  75,  76,  77,  78,  79,
    81,  82,  83,  84,  85,  87,  88,  89,  90,  91,  93,  94,  95,  97,  98,  99,
    100, 102, 103, 105, 106, 107, 109, 110, 111, 113, 114, 116, 117, 119, 120, 121,
    123, 124, 126, 127, 129, 130, 132, 133, 135, 137, 138, 140, 141, 143, 145, 146,
    148, 149, 151, 153, 154, 156, 158, 159, 161, 163, 165, 166, 168, 170, 172, 173,
    175, 177, 179, 181, 182, 184, 186, 188, 190, 192, 194, 196, 197, 199, 201, 203,
    205, 207, 209, 211, 213, 215, 217, 219, 221, 223, 225, 227, 229, 231, 234, 236,
    238, 240, 242, 244, 246, 248, 251, 253, 255, 255, 255, 255, 255, 255, 255, 255
};

/**
 * @class SensorStats
 * @brief Optimized sensor statistics tracker
 * 
 * Tracks min, max, and average values for sensor readings
 * with automatic overflow protection
 */
class SensorStats {
private:
    int16_t _min;
    int16_t _max;
    int32_t _sum;
    uint16_t _count;
    static constexpr uint16_t MAX_SAMPLES = 1000;
    static constexpr int16_t MIN_INITIAL = 32767;
    static constexpr int16_t MAX_INITIAL = -32768;

public:
    SensorStats() : _min(MIN_INITIAL), _max(MAX_INITIAL), _sum(0), _count(0) {}
    
    /**
     * @brief Adds a new sample to the statistics
     * @param value Sample value to add
     */
    void addSample(int16_t value) {
        if (value < _min) _min = value;
        if (value > _max) _max = value;
        
        _sum += value;
        _count++;

        if (_count >= MAX_SAMPLES) {
            int16_t avg = getAverage();
            _sum = avg;
            _count = 1;
        }
    }
    
    /**
     * @brief Gets the minimum recorded value
     * @return Minimum value or 0 if no samples
     */
    int16_t getMin() const { return _count > 0 ? _min : 0; }
    
    /**
     * @brief Gets the maximum recorded value
     * @return Maximum value or 0 if no samples
     */
    int16_t getMax() const { return _count > 0 ? _max : 0; }
    
    /**
     * @brief Gets the average of recorded values
     * @return Average value or 0 if no samples
     */
    int16_t getAverage() const { return _count > 0 ? (int16_t)(_sum / _count) : 0; }
    
    /**
     * @brief Resets all statistics to initial state
     */
    void reset() {
        _min = MIN_INITIAL;
        _max = MAX_INITIAL;
        _sum = 0;
        _count = 0;
    }
};

/**
 * @class SimpleLight
 * @brief Class for managing a simple on/off light
 * 
 * Manages a light connected to a digital pin with on/off
 * functionality and event response
 */
class SimpleLight : public IDevice, public IEventListener {
protected:
    uint8_t _pin;
    bool _state;
    // Integer-based brightness multiplier (0-100)
    static uint8_t& brightness_multiplier() {
        static uint8_t multiplier = 100;
        return multiplier;
    }

public:
    /**
     * @brief Constructor for simple light
     * @param name Device identifier name
     * @param pin Arduino pin to which the light is connected
     */
    SimpleLight(const char* name, uint8_t pin) 
        : IDevice(name, DeviceType::LightSimple), _pin(pin), _state(false) {
        pinMode(_pin, OUTPUT);
        digitalWrite(_pin, LOW);
        DeviceRegistry::instance().registerDevice(this);
        
        EventSystem::instance().addListener(this, EventType::ButtonPressed);
    }

    /**
     * @brief Destructor
     * 
     * Removes the device from registry and events
     */
    virtual ~SimpleLight() { 
        DeviceRegistry::instance().unregisterDevice(this);
        EventSystem::instance().removeListener(this);
    }

    /**
     * @brief Checks if the device is a light
     * @return true always
     */
    bool isLight() const override { return true; }
    
    /**
     * @brief Periodic update (not needed for simple light)
     */
    void update() override {}

    /**
     * @brief Toggles the light state (on/off)
     * 
     * Emits a DeviceStateChanged event
     */
    virtual void toggle() {
        _state = !_state;
        digitalWrite(_pin, _state ? HIGH : LOW);
        EventSystem::instance().emit(EventType::DeviceStateChanged, this, _state);
    }

    /**
     * @brief Gets the current light state
     * @return true if on, false if off
     */
    bool getState() const { return _state; }
    
    /**
     * @brief Handles received events
     * @param type Event type
     * @param source Event source device
     * @param value Value associated with the event
     */
    void handleEvent(EventType type, IDevice* source, int value) {
        if (type == EventType::ButtonPressed && source == this) {
            toggle();
        }
    }
    
    /**
     * @brief Sets the global brightness multiplier
     * @param multiplier Value between 0 and 100
     * 
     * Useful for implementing night mode (e.g., 20 for 20%)
     */
    static void setBrightnessMultiplier(uint8_t multiplier) {
        brightness_multiplier() = constrain(multiplier, 0, 100);
    }
};

/**
 * @class DimmableLight
 * @brief Light with brightness control (dimmer)
 * 
 * Extends SimpleLight adding the ability to adjust
 * brightness via PWM with smooth, non-blocking fading
 */
class DimmableLight : public SimpleLight {
protected:
    uint8_t _targetBrightness;
    uint8_t _currentBrightness;
    uint8_t _lastBrightness;
    unsigned long _lastUpdate;
    static constexpr uint8_t MS_PER_STEP = 4;

public:
    /**
     * @brief Constructor for dimmable light
     * @param name Device identifier name
     * @param pin Arduino PWM pin to which the light is connected
     */
    DimmableLight(const char* name, uint8_t pin) 
        : SimpleLight(name, pin), _targetBrightness(0), _currentBrightness(0), 
          _lastBrightness(100), _lastUpdate(0) {
        const_cast<DeviceType&>(type) = DeviceType::LightDimmable;
    }

    /**
     * @brief Sets the light brightness
     * @param level Brightness level (0-100)
     * 
     * The value is multiplied by brightness_multiplier
     * and converted to PWM. Emits a DeviceValueChanged event
     */
    virtual void setBrightness(uint8_t level) {
        _targetBrightness = constrain(level, 0, 100);
        if (_currentBrightness != _targetBrightness) {
            _lastUpdate = millis();
        }
        if (_targetBrightness > 0) {
            _lastBrightness = _targetBrightness;
        }
        
        _state = (_targetBrightness > 0);
        EventSystem::instance().emit(EventType::DeviceValueChanged, this, _targetBrightness);
    }

    /**
     * @brief Gets the current brightness level
     * @return Brightness level (0-100)
     */
    uint8_t getBrightness() const { return _targetBrightness; }
    
    /**
     * @brief Periodic update for smooth fading
     * 
     * Implements time-based fading with frame-skip support
     */
    void update() override {
        unsigned long now = millis();
        unsigned long diffTime = now - _lastUpdate;
        
        if (diffTime >= MS_PER_STEP && _currentBrightness != _targetBrightness) {
            uint8_t stepsToTake = diffTime / MS_PER_STEP;
            
            if (_currentBrightness < _targetBrightness) {
                uint8_t diff = _targetBrightness - _currentBrightness;
                _currentBrightness += min(stepsToTake, diff);
            } else {
                uint8_t diff = _currentBrightness - _targetBrightness;
                _currentBrightness -= min(stepsToTake, diff);
            }
            
            applyHardware();
            _lastUpdate = now - (diffTime % MS_PER_STEP);
        }
    }
    
    /**
     * @brief Toggles the light state while preserving brightness
     * 
     * If on, turns off saving the brightness.
     * If off, turns on restoring the previous brightness
     */
    void toggle() override {
        if (_state) {
            _lastBrightness = _targetBrightness;
            setBrightness(0);
        } else {
            setBrightness(_lastBrightness > 0 ? _lastBrightness : 100);
        }
    }
    
protected:
    /**
     * @brief Applies current brightness to hardware with gamma correction
     */
    void applyHardware() {
        if (_currentBrightness == 0) {
            analogWrite(_pin, PWM_MIN);
            return;
        }
        
        // Apply brightness multiplier: (value * multiplier) / 100
        uint16_t scaled = ((uint16_t)_currentBrightness * brightness_multiplier()) / 100;
        uint8_t linear = map(scaled, 0, 100, PWM_MIN, PWM_MAX);
        
        // Apply gamma correction
        uint8_t gamma_corrected = pgm_read_byte(&GAMMA_LUT[linear]);
        analogWrite(_pin, gamma_corrected);
    }
    
    /**
     * @brief Protected access to target brightness
     * @return Reference to target brightness value
     */
    uint8_t& targetBrightness() { return _targetBrightness; }
    
    /**
     * @brief Protected access to current brightness
     * @return Reference to current brightness value
     */
    uint8_t& currentBrightness() { return _currentBrightness; }
    
    /**
     * @brief Protected access to last saved brightness
     * @return Reference to last brightness value
     */
    uint8_t& lastBrightness() { return _lastBrightness; }
};

/**
 * @struct RGBColor
 * @brief Structure to represent an RGB color
 */
struct RGBColor { uint8_t r, g, b; };

/**
 * @enum RGBPreset
 * @brief Predefined color presets for RGB lights
 */
enum class RGBPreset : uint8_t {
    WARM_WHITE, COOL_WHITE, RED, GREEN, BLUE, OCEAN
};

const RGBColor PRESET_COLORS[] PROGMEM = {
    {255, 200, 100},
    {255, 255, 255},
    {255, 0, 0},
    {0, 255, 0},
    {0, 0, 255},
    {0, 150, 255}
};

/**
 * @class RGBLight
 * @brief RGB light with color and brightness control
 * 
 * Manages an RGB light with three separate PWM pins
 * and support for color presets with smooth fading
 */
class RGBLight : public DimmableLight {
private:
    uint8_t _pin_g, _pin_b;
    RGBColor _targetColor;
    RGBColor _currentColor;
    unsigned long _lastColorUpdate;
    static constexpr uint8_t MS_PER_STEP = 2;

public:
    /**
     * @brief Constructor for RGB light
     * @param name Device identifier name
     * @param pin_r PWM pin for red
     * @param pin_g PWM pin for green
     * @param pin_b PWM pin for blue
     */
    RGBLight(const char* name, uint8_t pin_r, uint8_t pin_g, uint8_t pin_b) 
        : DimmableLight(name, pin_r), _pin_g(pin_g), _pin_b(pin_b), 
          _targetColor({PWM_MAX, PWM_MAX, PWM_MAX}), _currentColor({0, 0, 0}), 
          _lastColorUpdate(0) {
        const_cast<DeviceType&>(type) = DeviceType::LightRGB;
        pinMode(_pin_g, OUTPUT);
        pinMode(_pin_b, OUTPUT);
    }

    /**
     * @brief Sets the RGB color
     * @param c RGBColor structure with R, G, B values
     * 
     * Emits a DeviceValueChanged event
     */
    void setColor(RGBColor c) {
        _targetColor = c;
        EventSystem::instance().emit(EventType::DeviceValueChanged, this, 0);
    }

    /**
     * @brief Gets the current target color
     * @return RGBColor structure with current values
     */
    RGBColor getColor() const { return _targetColor; }

    /**
     * @brief Channel-specific setters/getters for template compatibility
     */
    inline void setRed(uint8_t value) {
        _targetColor.r = value;
        EventSystem::instance().emit(EventType::DeviceValueChanged, this, 0);
    }
    
    inline uint8_t getRed() const { return _targetColor.r; }
    
    inline void setGreen(uint8_t value) {
        _targetColor.g = value;
        EventSystem::instance().emit(EventType::DeviceValueChanged, this, 0);
    }
    
    inline uint8_t getGreen() const { return _targetColor.g; }
    
    inline void setBlue(uint8_t value) {
        _targetColor.b = value;
        EventSystem::instance().emit(EventType::DeviceValueChanged, this, 0);
    }
    
    inline uint8_t getBlue() const { return _targetColor.b; }

    /**
     * @brief Sets a color from a predefined preset
     * @param preset RGBPreset enum value
     */
    void setPreset(RGBPreset preset) {
        RGBColor c;
        memcpy_P(&c, &PRESET_COLORS[(int)preset], sizeof(RGBColor));
        setColor(c);
    }

    /**
     * @brief Sets the RGB light brightness
     * @param level Brightness level (0-100)
     * 
     * Applies the brightness_multiplier and updates all three channels
     */
    void setBrightness(uint8_t level) override {
        if (_currentBrightness != _targetBrightness) {
            _lastUpdate = millis();
        }
        targetBrightness() = constrain(level, 0, 100);
        
        if (targetBrightness() > 0) {
            lastBrightness() = targetBrightness();
        }
        
        _state = (targetBrightness() > 0);
        EventSystem::instance().emit(EventType::DeviceValueChanged, this, targetBrightness());
    }

    /**
     * @brief Periodic update for smooth color and brightness fading
     * 
     * Implements time-based fading with frame-skip support for all channels
     */
    void update() override {
        unsigned long now = millis();
        
        // Update brightness
        unsigned long diffTime = now - _lastUpdate;
        if (diffTime >= MS_PER_STEP && currentBrightness() != targetBrightness()) {
            uint8_t stepsToTake = diffTime / MS_PER_STEP;
            
            if (currentBrightness() < targetBrightness()) {
                uint8_t diff = targetBrightness() - currentBrightness();
                currentBrightness() += min(stepsToTake, diff);
            } else {
                uint8_t diff = currentBrightness() - targetBrightness();
                currentBrightness() -= min(stepsToTake, diff);
            }
            
            _lastUpdate = now - (diffTime % MS_PER_STEP);
        }
        
        // Update colors
        unsigned long colorDiffTime = now - _lastColorUpdate;
        if (colorDiffTime >= MS_PER_STEP) {
            uint8_t stepsToTake = colorDiffTime / MS_PER_STEP;
            bool colorChanged = false;
            
            // Fade red channel
            if (_currentColor.r != _targetColor.r) {
                if (_currentColor.r < _targetColor.r) {
                    uint8_t diff = _targetColor.r - _currentColor.r;
                    _currentColor.r += min(stepsToTake, diff);
                } else {
                    uint8_t diff = _currentColor.r - _targetColor.r;
                    _currentColor.r -= min(stepsToTake, diff);
                }
                colorChanged = true;
            }
            
            // Fade green channel
            if (_currentColor.g != _targetColor.g) {
                if (_currentColor.g < _targetColor.g) {
                    uint8_t diff = _targetColor.g - _currentColor.g;
                    _currentColor.g += min(stepsToTake, diff);
                } else {
                    uint8_t diff = _currentColor.g - _targetColor.g;
                    _currentColor.g -= min(stepsToTake, diff);
                }
                colorChanged = true;
            }
            
            // Fade blue channel
            if (_currentColor.b != _targetColor.b) {
                if (_currentColor.b < _targetColor.b) {
                    uint8_t diff = _targetColor.b - _currentColor.b;
                    _currentColor.b += min(stepsToTake, diff);
                } else {
                    uint8_t diff = _currentColor.b - _targetColor.b;
                    _currentColor.b -= min(stepsToTake, diff);
                }
                colorChanged = true;
            }
            
            if (colorChanged) {
                _lastColorUpdate = now - (colorDiffTime % MS_PER_STEP);
            }
        }
        
        // Apply to hardware if anything changed
        if (diffTime >= MS_PER_STEP || colorDiffTime >= MS_PER_STEP) {
            applyColor();
        }
    }

    /**
     * @brief Toggles the RGB light state
     * 
     * Preserves color and previous brightness
     */
    void toggle() override {
        if (_state) {
            lastBrightness() = targetBrightness();
            targetBrightness() = 0;
            _state = false;
        } else {
            targetBrightness() = lastBrightness() > 0 ? lastBrightness() : 100;
            _state = true;
        }
        EventSystem::instance().emit(EventType::DeviceStateChanged, this, _state);
    }

private:
    /**
     * @brief Applies the current color to PWM pins with gamma correction
     * 
     * Takes into account brightness and brightness_multiplier
     */
    void applyColor() {
        if (!_state || currentBrightness() == 0) {
            analogWrite(_pin, PWM_MIN);
            analogWrite(_pin_g, PWM_MIN);
            analogWrite(_pin_b, PWM_MIN);
            return;
        }
        
        // Apply brightness multiplier: (brightness * multiplier) / 100
        uint16_t factor = ((uint16_t)currentBrightness() * brightness_multiplier()) / 100;
        
        // Scale each channel by brightness factor
        uint8_t r_scaled = ((uint16_t)_currentColor.r * factor) / 100;
        uint8_t g_scaled = ((uint16_t)_currentColor.g * factor) / 100;
        uint8_t b_scaled = ((uint16_t)_currentColor.b * factor) / 100;
        
        // Apply gamma correction
        analogWrite(_pin, pgm_read_byte(&GAMMA_LUT[r_scaled]));
        analogWrite(_pin_g, pgm_read_byte(&GAMMA_LUT[g_scaled]));
        analogWrite(_pin_b, pgm_read_byte(&GAMMA_LUT[b_scaled]));
    }
};

/**
 * @enum OutsideMode
 * @brief Operating modes for outdoor light
 */
enum class OutsideMode : uint8_t { OFF, ON, AUTO_LIGHT, AUTO_MOTION };

/**
 * @class OutsideLight
 * @brief Outdoor light with sensors and automation
 * 
 * Manages an outdoor light that can operate in manual
 * or automatic mode based on light and motion sensors
 */
class OutsideLight : public SimpleLight {
private:
    OutsideMode _mode;
    LightSensor* _photo;
    MovementSensor* _motion;
    static constexpr int DARKNESS_THRESHOLD = 30;

public:
    /**
     * @brief Constructor for outdoor light
     * @param name Device identifier name
     * @param pin Arduino pin to which the light is connected
     * @param photo Pointer to light sensor
     * @param motion Pointer to motion sensor
     */
    OutsideLight(const char* name, uint8_t pin, LightSensor* photo, MovementSensor* motion)
        : SimpleLight(name, pin), _mode(OutsideMode::OFF), _photo(photo), _motion(motion) {
        const_cast<DeviceType&>(type) = DeviceType::LightOutside;
    }

    /**
     * @brief Sets the operating mode
     * @param mode Desired mode (OFF, ON, AUTO_LIGHT, AUTO_MOTION)
     */
    void setMode(OutsideMode mode) {
        _mode = mode;
        update();
    }

    /**
     * @brief Periodic state update
     * 
     * Evaluates sensors and updates state based on current mode
     */
    void update() override {
        bool targetState = false;
        switch(_mode) {
            case OutsideMode::OFF: targetState = false; break;
            case OutsideMode::ON: targetState = true; break;
            case OutsideMode::AUTO_LIGHT:
                targetState = (_photo->getValue() < DARKNESS_THRESHOLD);
                break;
            case OutsideMode::AUTO_MOTION:
                targetState = (_photo->getValue() < DARKNESS_THRESHOLD && _motion->getValue());
                break;
        }
        
        if (targetState != _state) {
            _state = targetState;
            digitalWrite(_pin, _state ? HIGH : LOW);
            EventSystem::instance().emit(EventType::DeviceStateChanged, this, _state);
        }
    }

    /**
     * @brief Toggles between ON and OFF modes
     */
    void toggle() override {
        if (_state) {
            _mode = OutsideMode::OFF;
        } else {
            _mode = OutsideMode::ON;
        }
    }
};

/**
 * @class TemperatureSensor
 * @brief Temperature sensor with statistics (NO FLOATS)
 * 
 * Uses an LM75 sensor to detect temperature in decicelsius
 * and maintains min/max/average statistics
 */
class TemperatureSensor : public IDevice {
private:
    int16_t _temperature;  // Decicelsius (tenths of degree)
    unsigned long _lastRead;
    SensorStats _stats;
    LM75Sensor _lm75;
    static constexpr unsigned long UPDATE_INTERVAL_MS = 2000;

public:
    /**
     * @brief Constructor for temperature sensor
     * @param name Device identifier name
     */
    TemperatureSensor(const char* name) 
        : IDevice(name, DeviceType::SensorTemperature), _temperature(0), _lastRead(0), _lm75(name) {
        _lm75.begin();
        DeviceRegistry::instance().registerDevice(this);
    }

    /**
     * @brief Checks if the device is a sensor
     * @return true always
     */
    bool isSensor() const override { return true; }

    /**
     * @brief Periodic reading update
     * 
     * Reads temperature and updates statistics.
     * Emits a SensorUpdated event
     */
    void update() override {
        if (millis() - _lastRead > UPDATE_INTERVAL_MS) {
            _lastRead = millis();
            _temperature = _lm75.getValue();  // Already in decicelsius
            _stats.addSample(_temperature);
            EventSystem::instance().emit(EventType::SensorUpdated, this, _temperature);
        }
    }

    /**
     * @brief Gets the current temperature in decicelsius
     * @return Temperature * 10 (e.g., 205 = 20.5°C)
     */
    int16_t getTemperature() const { return _temperature; }
    
    /**
     * @brief Gets the sensor statistics
     * @return Reference to SensorStats object
     */
    SensorStats& getStats() { return _stats; }
};

/**
 * @class PhotoresistorSensor
 * @brief Light sensor with calibration
 * 
 * Uses a photoresistor to measure light level
 * with min/max calibration capability
 */
class PhotoresistorSensor : public IDevice {
private:
    int _lightLevel;
    unsigned long _lastRead;
    LightSensor _photoSensor;
    SensorStats _stats;
    static constexpr unsigned long UPDATE_INTERVAL_MS = 10;
    static constexpr int CHANGE_THRESHOLD = 2;

public:
    /**
     * @brief Constructor for light sensor
     * @param name Device identifier name
     * @param pin Arduino analog pin
     */
    PhotoresistorSensor(const char* name, uint8_t pin) 
        : IDevice(name, DeviceType::SensorLight), _lightLevel(0), _lastRead(0), _photoSensor(name, pin) {
        DeviceRegistry::instance().registerDevice(this);
    }

    /**
     * @brief Checks if the device is a sensor
     * @return true always
     */
    bool isSensor() const override { return true; }

    /**
     * @brief Periodic reading update
     * 
     * Reads light level and updates statistics if the change
     * exceeds threshold. Emits a SensorUpdated event
     */
    void update() override {
        if (millis() - _lastRead > UPDATE_INTERVAL_MS) {
            _lastRead = millis();
            int newValue = _photoSensor.getValue();
            
            if (abs(newValue - _lightLevel) > CHANGE_THRESHOLD) {
                _lightLevel = newValue;
                _stats.addSample((int16_t)_lightLevel);
                EventSystem::instance().emit(EventType::SensorUpdated, this, _lightLevel);
            }
        }
    }

    /**
     * @brief Gets the current light level
     * @return Light value (0-100)
     */
    int getValue() const { return _lightLevel; }
    
    /**
     * @brief Gets the sensor statistics
     * @return Reference to SensorStats object
     */
    SensorStats& getStats() { return _stats; }
    
    /**
     * @brief Calibrates minimum value with current reading
     */
    void calibrateCurrentAsMin() {
        int raw = _photoSensor.getRaw();
        _photoSensor.setRawMin(raw);
    }
    
    /**
     * @brief Calibrates maximum value with current reading
     */
    void calibrateCurrentAsMax() {
        int raw = _photoSensor.getRaw();
        _photoSensor.setRawMax(raw);
    }
    
    /**
     * @brief Gets the calibrated raw minimum value
     * @return Raw minimum value
     */
    int getRawMin() const { return _photoSensor.getRawMin(); }
    
    /**
     * @brief Gets the calibrated raw maximum value
     * @return Raw maximum value
     */
    int getRawMax() const { return _photoSensor.getRawMax(); }
};

/**
 * @class PIRSensorDevice
 * @brief PIR motion sensor
 * 
 * Detects motion using a PIR sensor and emits
 * events when state changes
 */
class PIRSensorDevice : public IDevice {
private:
    bool _motionDetected;
    unsigned long _lastRead;
    MovementSensor _pirSensor;
    static constexpr unsigned long UPDATE_INTERVAL_MS = 500;

public:
    /**
     * @brief Constructor for PIR sensor
     * @param name Device identifier name
     * @param pin Arduino digital pin
     */
    PIRSensorDevice(const char* name, uint8_t pin) 
        : IDevice(name, DeviceType::SensorPIR), _motionDetected(false), _lastRead(0), _pirSensor(name, pin) {
        DeviceRegistry::instance().registerDevice(this);
    }

    /**
     * @brief Checks if the device is a sensor
     * @return true always
     */
    bool isSensor() const override { return true; }

    /**
     * @brief Periodic reading update
     * 
     * Checks PIR sensor state and emits
     * SensorUpdated event only when state changes
     */
    void update() override {
        if (millis() - _lastRead > UPDATE_INTERVAL_MS) {
            _lastRead = millis();
            bool newValue = _pirSensor.getValue();
            
            if (newValue != _motionDetected) {
                _motionDetected = newValue;
                EventSystem::instance().emit(EventType::SensorUpdated, this, _motionDetected ? 1 : 0);
            }
        }
    }

    /**
     * @brief Checks if motion is detected
     * @return true if motion detected, false otherwise
     */
    bool isMotionDetected() const { return _motionDetected; }
};

/**
 * @class DeviceFactory
 * @brief Factory for creating devices
 * 
 * Provides static methods to create instances of various
 * device types with controlled dynamic allocation
 */
class DeviceFactory {
public:
    /**
     * @brief Creates a simple on/off light
     * @param name Device name
     * @param pin Arduino pin
     */
    static void createSimpleLight(const char* name, uint8_t pin) {
        new SimpleLight(name, pin);
    }

    /**
     * @brief Creates a dimmable light
     * @param name Device name
     * @param pin Arduino PWM pin
     */
    static void createDimmableLight(const char* name, uint8_t pin) {
        new DimmableLight(name, pin);
    }

    /**
     * @brief Creates a temperature sensor
     * @param name Device name
     */
    static void createTemperatureSensor(const char* name) {
        new TemperatureSensor(name);
    }
    
    /**
     * @brief Creates an RGB light
     * @param name Device name
     * @param r PWM pin for red
     * @param g PWM pin for green
     * @param b PWM pin for blue
     */
    static void createRGBLight(const char* name, uint8_t r, uint8_t g, uint8_t b) {
        new RGBLight(name, r, g, b);
    }
    
    /**
     * @brief Creates an automatic outdoor light
     * @param name Device name
     * @param pin Arduino pin
     * @param p Pointer to light sensor
     * @param m Pointer to motion sensor
     */
    static void createOutsideLight(const char* name, uint8_t pin, LightSensor* p, MovementSensor* m) {
        new OutsideLight(name, pin, p, m);
    }
    
    /**
     * @brief Creates a light sensor
     * @param name Device name
     * @param pin Arduino analog pin
     */
    static void createPhotoresistorSensor(const char* name, uint8_t pin) {
        new PhotoresistorSensor(name, pin);
    }
    
    /**
     * @brief Creates a PIR motion sensor
     * @param name Device name
     * @param pin Arduino digital pin
     */
    static void createPIRSensor(const char* name, uint8_t pin) {
        new PIRSensorDevice(name, pin);
    }
};

#endif