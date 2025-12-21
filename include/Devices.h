/**
 * @file Devices.h
 * @brief Device class declarations for lights, sensors, and automation
 * @author Andrea Bortolotti
 * @version 2.0
 * 
 * @details Provides all device abstractions for the smart home system:
 * - Light devices (Simple, Dimmable, RGB, Outside)
 * - Sensor devices (Temperature, Light, PIR, RAM, VCC, LoopTime)
 * - Device factory for convenient instantiation
 * 
 * @ingroup Devices
 */
#ifndef DEVICES_H
#define DEVICES_H

#include "CoreSystem.h"
#include "sensors.h"
#include <avr/pgmspace.h>

#define PWM_MIN 0   ///< Minimum PWM output value
#define PWM_MAX 255 ///< Maximum PWM output value

/**
 * @brief Gamma correction lookup table (256 entries)
 * @details Safe low-end curve: input 1 maps to output 1 (not 0) to avoid black crush.
 * Generated using gamma = 2.2
 * @ingroup Devices
 */
extern const uint8_t GAMMA_LUT[256] PROGMEM;

/**
 * @class SensorStats
 * @brief Optimized sensor statistics tracker
 * @ingroup Devices
 * 
 * @details Tracks min, max, and average values for sensor readings
 * with automatic overflow protection. Resets after MAX_SAMPLES to
 * prevent integer overflow in sum accumulator.
 */
class SensorStats {
private:
    int16_t _min;                                  ///< Minimum recorded value
    int16_t _max;                                  ///< Maximum recorded value
    int32_t _sum;                                  ///< Running sum for average calculation
    uint16_t _count;                               ///< Number of samples recorded
    static constexpr uint16_t MAX_SAMPLES = 1000;  ///< Reset threshold
    static constexpr int16_t MIN_INITIAL = 32767;  ///< Initial minimum value
    static constexpr int16_t MAX_INITIAL = -32768; ///< Initial maximum value

public:
    /**
     * @brief Default constructor
     */
    SensorStats();
    
    /**
     * @brief Adds a new sample to the statistics
     * @param value Sample value to add
     */
    void addSample(int16_t value);
    
    /**
     * @brief Gets the minimum recorded value
     * @return Minimum value or 0 if no samples
     */
    int16_t getMin() const;
    
    /**
     * @brief Gets the maximum recorded value
     * @return Maximum value or 0 if no samples
     */
    int16_t getMax() const;
    
    /**
     * @brief Gets the average of recorded values
     * @return Average value or 0 if no samples
     */
    int16_t getAverage() const;
    
    /**
     * @brief Resets all statistics to initial state
     */
    void reset();
};

/**
 * @class SimpleLight
 * @brief Class for managing a simple on/off light
 * @ingroup Devices
 */
class SimpleLight : public IDevice, public IEventListener {
protected:
    uint8_t _pin;   ///< Arduino pin number
    bool _state;    ///< Current on/off state

public:
    /**
     * @brief Constructor for simple light
     * @param name Device identifier name (Flash string)
     * @param pin Arduino pin to which the light is connected
     */
    SimpleLight(const __FlashStringHelper* name, uint8_t pin);

    /**
     * @brief Destructor
     */
    virtual ~SimpleLight();

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
     */
    virtual void toggle();

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
    void handleEvent(EventType type, IDevice* source, int value) override;
};

/**
 * @class DimmableLight
 * @brief Light with brightness control (dimmer)
 * @ingroup Devices
 */
class DimmableLight : public SimpleLight {
protected:
    uint8_t _targetBrightness;    ///< Target brightness level (0-100)
    uint8_t _currentBrightness;   ///< Current fading brightness (0-100)
    uint8_t _lastBrightness;      ///< Saved brightness for toggle restore
    unsigned long _lastUpdate;    ///< Timestamp of last fade step
    uint8_t _lastMultiplier;      ///< Cached multiplier for change detection
    static constexpr uint8_t MS_PER_STEP = 4;  ///< Fade step interval in ms
    
    /**
     * @brief Gets reference to global brightness multiplier
     * @return Reference to static multiplier value
     */
    static uint8_t& brightness_multiplier();

public:
    /**
     * @brief Constructor for dimmable light
     * @param name Device identifier name (Flash string)
     * @param pin Arduino PWM pin to which the light is connected
     */
    DimmableLight(const __FlashStringHelper* name, uint8_t pin);

    /**
     * @brief Sets the light brightness
     * @param level Brightness level (0-100)
     */
    virtual void setBrightness(uint8_t level);

    /**
     * @brief Gets the current brightness level
     * @return Brightness level (0-100)
     */
    uint8_t getBrightness() const { return _targetBrightness; }
    
    /**
     * @brief Periodic update for smooth fading
     */
    void update() override;
    
    /**
     * @brief Toggles the light state while preserving brightness
     */
    void toggle() override;
    
    /**
     * @brief Sets the global brightness multiplier
     * @param multiplier Value between 0 and 100
     */
    static void setBrightnessMultiplier(uint8_t multiplier);
    
protected:
    /**
     * @brief Applies current brightness to hardware with gamma correction
     */
    void applyHardware();
    
    /**
     * @brief Gets reference to target brightness
     * @return Reference to target brightness member
     */
    uint8_t& targetBrightness() { return _targetBrightness; }
    
    /**
     * @brief Gets reference to current brightness
     * @return Reference to current brightness member
     */
    uint8_t& currentBrightness() { return _currentBrightness; }
    
    /**
     * @brief Gets reference to last brightness
     * @return Reference to last brightness member
     */
    uint8_t& lastBrightness() { return _lastBrightness; }
};

/**
 * @struct RGBColor
 * @brief Structure to represent an RGB color
 * @ingroup Devices
 */
struct RGBColor { 
    uint8_t r;  ///< Red channel (0-255)
    uint8_t g;  ///< Green channel (0-255)
    uint8_t b;  ///< Blue channel (0-255)
};

/**
 * @brief Predefined color presets for RGB lights
 * @ingroup Devices
 */
enum class RGBPreset : uint8_t {
    WARM_WHITE,  ///< Warm white (2700K approximation)
    COOL_WHITE,  ///< Cool white (6500K approximation)
    RED,         ///< Pure red
    GREEN,       ///< Pure green
    BLUE,        ///< Pure blue
    OCEAN        ///< Ocean blue-green
};

/**
 * @brief Preset color values stored in PROGMEM
 * @ingroup Devices
 */
extern const RGBColor PRESET_COLORS[] PROGMEM;

/**
 * @class RGBLight
 * @brief RGB light with color and brightness control
 * @ingroup Devices
 */
class RGBLight : public DimmableLight {
private:
    uint8_t _pin_g;                  ///< Green channel PWM pin
    uint8_t _pin_b;                  ///< Blue channel PWM pin
    RGBColor _targetColor;           ///< Target color for fading
    RGBColor _currentColor;          ///< Current fading color
    unsigned long _lastColorUpdate;  ///< Timestamp of last color fade step

public:
    /**
     * @brief Constructor for RGB light
     * @param name Device identifier name (Flash string)
     * @param pin_r PWM pin for red channel
     * @param pin_g PWM pin for green channel
     * @param pin_b PWM pin for blue channel
     */
    RGBLight(const __FlashStringHelper* name, uint8_t pin_r, uint8_t pin_g, uint8_t pin_b);

    /**
     * @brief Sets the RGB color
     * @param c RGBColor structure with R, G, B values
     */
    void setColor(RGBColor c);

    /**
     * @brief Gets the current target color
     * @return RGBColor structure with current values
     */
    RGBColor getColor() const { return _targetColor; }

    /**
     * @brief Sets red channel value
     * @param value Red intensity (0-255)
     */
    void setRed(uint8_t value);
    
    /**
     * @brief Gets red channel value
     * @return Red intensity (0-255)
     */
    uint8_t getRed() const { return _targetColor.r; }
    
    /**
     * @brief Sets green channel value
     * @param value Green intensity (0-255)
     */
    void setGreen(uint8_t value);
    
    /**
     * @brief Gets green channel value
     * @return Green intensity (0-255)
     */
    uint8_t getGreen() const { return _targetColor.g; }
    
    /**
     * @brief Sets blue channel value
     * @param value Blue intensity (0-255)
     */
    void setBlue(uint8_t value);
    
    /**
     * @brief Gets blue channel value
     * @return Blue intensity (0-255)
     */
    uint8_t getBlue() const { return _targetColor.b; }

    /**
     * @brief Sets a color from a predefined preset
     * @param preset RGBPreset enum value
     */
    void setPreset(RGBPreset preset);

    /**
     * @brief Sets overall brightness
     * @param level Brightness level (0-100)
     */
    void setBrightness(uint8_t level) override;
    
    /**
     * @brief Periodic update for smooth color fading
     */
    void update() override;
    
    /**
     * @brief Toggles the light state while preserving color
     */
    void toggle() override;

private:
    /**
     * @brief Applies current color to hardware PWM outputs
     */
    void applyColor();
};

/**
 * @brief Operating modes for outdoor light
 * @ingroup Devices
 */
enum class OutsideMode : uint8_t { 
    OFF,         ///< Light always off
    ON,          ///< Light always on
    AUTO_LIGHT,  ///< Automatic based on ambient light
    AUTO_MOTION  ///< Automatic based on light + motion
};

/**
 * @class TemperatureSensor
 * @brief Temperature sensor device with statistics (NO FLOATS)
 * @ingroup Devices
 */
class TemperatureSensor : public IDevice {
private:
    int16_t _temperature;         ///< Current temperature in decicelsius
    unsigned long _lastRead;      ///< Timestamp of last reading
    SensorStats _stats;           ///< Statistics tracker
    LM75Sensor _lm75;             ///< Low-level sensor driver
    static constexpr unsigned long UPDATE_INTERVAL_MS = 2000;

public:
    /**
     * @brief Constructor for temperature sensor
     * @param name Device identifier name (Flash string)
     */
    explicit TemperatureSensor(const __FlashStringHelper* name);

    /**
     * @brief Checks if device is a sensor
     * @return true always
     */
    bool isSensor() const override { return true; }
    
    /**
     * @brief Periodic update - reads sensor at defined interval
     */
    void update() override;
    
    /**
     * @brief Gets current temperature
     * @return Temperature in decicelsius
     */
    int16_t getTemperature() const { return _temperature; }
    
    /**
     * @brief Gets statistics tracker
     * @return Reference to SensorStats object
     */
    SensorStats& getStats() { return _stats; }
};

/**
 * @class PhotoresistorSensor
 * @brief Light sensor device with calibration and statistics
 * @ingroup Devices
 */
class PhotoresistorSensor : public IDevice {
private:
    int _lightLevel;               ///< Current light level (0-100)
    unsigned long _lastRead;       ///< Timestamp of last reading
    LightSensor _photoSensor;      ///< Low-level sensor driver
    SensorStats _stats;            ///< Statistics tracker
    static constexpr unsigned long UPDATE_INTERVAL_MS = 250;
    static constexpr int CHANGE_THRESHOLD = 2;

public:
    /**
     * @brief Constructor for light sensor
     * @param name Device identifier name (Flash string)
     * @param pin Arduino analog pin
     */
    PhotoresistorSensor(const __FlashStringHelper* name, uint8_t pin);

    /**
     * @brief Checks if device is a sensor
     * @return true always
     */
    bool isSensor() const override { return true; }
    
    /**
     * @brief Periodic update - reads sensor at defined interval
     */
    void update() override;
    
    /**
     * @brief Gets current light level
     * @return Light level 0-100%
     */
    int getValue() const { return _lightLevel; }
    
    /**
     * @brief Gets statistics tracker
     * @return Reference to SensorStats object
     */
    SensorStats& getStats() { return _stats; }
    
    /**
     * @brief Calibrates current reading as minimum (dark)
     */
    void calibrateCurrentAsMin();
    
    /**
     * @brief Calibrates current reading as maximum (bright)
     */
    void calibrateCurrentAsMax();
    
    /**
     * @brief Gets calibrated minimum value
     * @return Raw ADC minimum
     */
    int getRawMin() const { return _photoSensor.getRawMin(); }
    
    /**
     * @brief Gets calibrated maximum value
     * @return Raw ADC maximum
     */
    int getRawMax() const { return _photoSensor.getRawMax(); }
};

/**
 * @class PIRSensorDevice
 * @brief PIR motion sensor device
 * @ingroup Devices
 */
class PIRSensorDevice : public IDevice {
private:
    bool _motionDetected;          ///< Current motion state
    unsigned long _lastRead;       ///< Timestamp of last reading
    MovementSensor _pirSensor;     ///< Low-level sensor driver
    static constexpr unsigned long UPDATE_INTERVAL_MS = 500;

public:
    /**
     * @brief Constructor for PIR sensor
     * @param name Device identifier name (Flash string)
     * @param pin Arduino digital pin
     */
    PIRSensorDevice(const __FlashStringHelper* name, uint8_t pin);

    /**
     * @brief Checks if device is a sensor
     * @return true always
     */
    bool isSensor() const override { return true; }
    
    /**
     * @brief Periodic update - reads sensor at defined interval
     */
    void update() override;
    
    /**
     * @brief Gets motion detection state
     * @return true if motion detected
     */
    bool isMotionDetected() const { return _motionDetected; }
};

/**
 * @class RamSensorDevice
 * @brief RAM usage monitoring device with statistics
 * @ingroup Devices
 */
class RamSensorDevice : public IDevice {
private:
    int16_t _freeRam;              ///< Current free RAM in bytes
    int16_t _lastReported;         ///< Last reported value (for threshold)
    unsigned long _lastRead;       ///< Timestamp of last reading
    SensorStats _stats;            ///< Statistics tracker
    RamUsageSensor _ramSensor;     ///< Low-level sensor driver
    static constexpr unsigned long UPDATE_INTERVAL_MS = 10000;
    static constexpr int16_t CHANGE_THRESHOLD = 16;

public:
    /**
     * @brief Constructor for RAM sensor device
     * @param name Device identifier name (Flash string)
     */
    explicit RamSensorDevice(const __FlashStringHelper* name);

    /**
     * @brief Checks if device is a sensor
     * @return true always
     */
    bool isSensor() const override { return true; }
    
    /**
     * @brief Periodic update - reads sensor at defined interval
     */
    void update() override;
    
    /**
     * @brief Gets current free RAM
     * @return Free RAM in bytes
     */
    int16_t getValue() const { return _freeRam; }
    
    /**
     * @brief Gets statistics tracker
     * @return Reference to SensorStats object
     */
    SensorStats& getStats() { return _stats; }
};

/**
 * @class VccSensorDevice
 * @brief VCC voltage monitoring device with statistics
 * @ingroup Devices
 */
class VccSensorDevice : public IDevice {
private:
    int16_t _vcc;                  ///< Current VCC in millivolts
    unsigned long _lastRead;       ///< Timestamp of last reading
    SensorStats _stats;            ///< Statistics tracker
    VccSensor _vccSensor;          ///< Low-level sensor driver
    static constexpr unsigned long UPDATE_INTERVAL_MS = 10000;

public:
    /**
     * @brief Constructor for VCC sensor device
     * @param name Device identifier name (Flash string)
     */
    explicit VccSensorDevice(const __FlashStringHelper* name);

    /**
     * @brief Checks if device is a sensor
     * @return true always
     */
    bool isSensor() const override { return true; }
    
    /**
     * @brief Periodic update - reads sensor at defined interval
     */
    void update() override;
    
    /**
     * @brief Gets current VCC voltage
     * @return VCC in millivolts
     */
    int16_t getValue() const { return _vcc; }
    
    /**
     * @brief Gets statistics tracker
     * @return Reference to SensorStats object
     */
    SensorStats& getStats() { return _stats; }
};

/**
 * @class LoopTimeSensorDevice
 * @brief Loop execution time monitoring device with statistics
 * @ingroup Devices
 */
class LoopTimeSensorDevice : public IDevice {
private:
    int16_t _loopTime;             ///< Current loop time in microseconds
    unsigned long _lastRead;       ///< Timestamp of last reading
    SensorStats _stats;            ///< Statistics tracker
    LoopTimeSensor _loopSensor;    ///< Low-level sensor driver
    static constexpr unsigned long UPDATE_INTERVAL_MS = 1000;

public:
    /**
     * @brief Constructor for loop time sensor device
     * @param name Device identifier name (Flash string)
     */
    explicit LoopTimeSensorDevice(const __FlashStringHelper* name);

    /**
     * @brief Checks if device is a sensor
     * @return true always
     */
    bool isSensor() const override { return true; }
    
    /**
     * @brief Registers loop iteration time (static accessor)
     * @param microseconds Duration of the loop iteration
     */
    static void registerLoopTime(uint16_t microseconds);
    
    /**
     * @brief Periodic update - reads sensor at defined interval
     */
    void update() override;
    
    /**
     * @brief Gets current loop time
     * @return Loop time in microseconds
     */
    int16_t getValue() const { return _loopTime; }
    
    /**
     * @brief Gets statistics tracker
     * @return Reference to SensorStats object
     */
    SensorStats& getStats() { return _stats; }
};

/**
 * @class OutsideLight
 * @brief Outdoor light with sensors and automation
 * @ingroup Devices
 */
class OutsideLight : public SimpleLight {
private:
    OutsideMode _mode;              ///< Current operating mode
    PhotoresistorSensor* _photo;    ///< Linked light sensor
    PIRSensorDevice* _motion;       ///< Linked motion sensor
    static constexpr int DARKNESS_THRESHOLD = 30;

    /**
     * @brief Evaluates light state based on mode and sensors
     */
    void evaluateState();

public:
    /**
     * @brief Constructor for outdoor light
     * @param name Device identifier name (Flash string)
     * @param pin Arduino pin to which the light is connected
     * @param photo Pointer to light sensor
     * @param motion Pointer to motion sensor
     */
    OutsideLight(const __FlashStringHelper* name, uint8_t pin, 
                 PhotoresistorSensor* photo, PIRSensorDevice* motion);

    /**
     * @brief Destructor
     */
    virtual ~OutsideLight();

    /**
     * @brief Sets the operating mode
     * @param mode OutsideMode enum value
     */
    void setMode(OutsideMode mode);
    
    /**
     * @brief Periodic update (handled via events)
     */
    void update() override {}
    
    /**
     * @brief Toggles between ON and OFF modes
     */
    void toggle() override;
    
    /**
     * @brief Handles sensor events for automation
     * @param type Event type
     * @param source Event source device
     * @param value Value associated with the event
     */
    void handleEvent(EventType type, IDevice* source, int value) override;
};

/**
 * @class DeviceFactory
 * @brief Factory for creating and registering devices
 * @ingroup Devices
 * 
 * @details Provides static methods to create devices and automatically
 * register them with the DeviceRegistry. Simplifies device initialization.
 */
class DeviceFactory {
public:
    /**
     * @brief Creates a simple on/off light
     * @param name Device name (Flash string)
     * @param pin Arduino digital pin
     */
    static void createSimpleLight(const __FlashStringHelper* name, uint8_t pin);
    
    /**
     * @brief Creates a dimmable light
     * @param name Device name (Flash string)
     * @param pin Arduino PWM pin
     */
    static void createDimmableLight(const __FlashStringHelper* name, uint8_t pin);
    
    /**
     * @brief Creates a temperature sensor
     * @param name Device name (Flash string)
     */
    static void createTemperatureSensor(const __FlashStringHelper* name);
    
    /**
     * @brief Creates an RGB light
     * @param name Device name (Flash string)
     * @param r Red channel PWM pin
     * @param g Green channel PWM pin
     * @param b Blue channel PWM pin
     */
    static void createRGBLight(const __FlashStringHelper* name, uint8_t r, uint8_t g, uint8_t b);
    
    /**
     * @brief Creates an outdoor light with sensors
     * @param name Device name (Flash string)
     * @param pin Arduino digital pin
     * @param p Pointer to light sensor
     * @param m Pointer to motion sensor
     * @return Pointer to created OutsideLight
     */
    static OutsideLight* createOutsideLight(const __FlashStringHelper* name, uint8_t pin, 
                                            PhotoresistorSensor* p, PIRSensorDevice* m);
    
    /**
     * @brief Creates a photoresistor sensor
     * @param name Device name (Flash string)
     * @param pin Arduino analog pin
     * @return Pointer to created PhotoresistorSensor
     */
    static PhotoresistorSensor* createPhotoresistorSensor(const __FlashStringHelper* name, uint8_t pin);
    
    /**
     * @brief Creates a PIR motion sensor
     * @param name Device name (Flash string)
     * @param pin Arduino digital pin
     * @return Pointer to created PIRSensorDevice
     */
    static PIRSensorDevice* createPIRSensor(const __FlashStringHelper* name, uint8_t pin);
    
    /**
     * @brief Creates a RAM usage sensor
     * @param name Device name (Flash string)
     */
    static void createRamSensor(const __FlashStringHelper* name);
    
    /**
     * @brief Creates a VCC voltage sensor
     * @param name Device name (Flash string)
     */
    static void createVoltageSensor(const __FlashStringHelper* name);
    
    /**
     * @brief Creates a loop time sensor
     * @param name Device name (Flash string)
     */
    static void createLoopTimeSensor(const __FlashStringHelper* name);
};

#endif