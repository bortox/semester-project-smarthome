/**
 * @file Devices.cpp
 * @brief Implementation of device classes
 * @author Andrea Bortolotti
 * @version 2.0
 * @ingroup Devices
 */
#include <Arduino.h>
#include "Devices.h"

const uint8_t GAMMA_LUT[256] PROGMEM = {
    0,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
    1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
    1,   2,   2,   2,   2,   2,   2,   2,   2,   3,   3,   3,   3,   3,   4,   4,
    4,   4,   5,   5,   5,   5,   6,   6,   6,   6,   7,   7,   7,   8,   8,   8,
    9,   9,   9,  10,  10,  11,  11,  11,  12,  12,  13,  13,  14,  14,  14,  15,
   15,  16,  16,  17,  17,  18,  18,  19,  19,  20,  21,  21,  22,  22,  23,  24,
   24,  25,  25,  26,  27,  27,  28,  29,  29,  30,  31,  31,  32,  33,  34,  34,
   35,  36,  37,  37,  38,  39,  40,  41,  41,  42,  43,  44,  45,  46,  47,  47,
   48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  62,  63,
   64,  65,  66,  67,  68,  69,  70,  71,  73,  74,  75,  76,  77,  78,  80,  81,
   82,  83,  84,  86,  87,  88,  89,  91,  92,  93,  95,  96,  97,  99, 100, 101,
  103, 104, 106, 107, 108, 110, 111, 113, 114, 116, 117, 119, 120, 122, 123, 125,
  126, 128, 130, 131, 133, 134, 136, 138, 139, 141, 143, 144, 146, 148, 149, 151,
  153, 155, 156, 158, 160, 162, 164, 165, 167, 169, 171, 173, 175, 177, 178, 180,
  182, 184, 186, 188, 190, 192, 194, 196, 198, 200, 202, 204, 206, 208, 210, 212,
  215, 217, 219, 221, 223, 225, 228, 230, 232, 234, 237, 239, 241, 243, 246, 255
};

const RGBColor PRESET_COLORS[] PROGMEM = {
    {255, 180, 100},  ///< WARM_WHITE
    {255, 255, 255},  ///< COOL_WHITE
    {255,   0,   0},  ///< RED
    {  0, 255,   0},  ///< GREEN
    {  0,   0, 255},  ///< BLUE
    {  0, 100, 180}   ///< OCEAN
};

SensorStats::SensorStats() {
    reset();
}

// cppcheck-suppress unusedFunction
void SensorStats::addSample(int16_t value) {
    if (_count >= MAX_SAMPLES) {
        reset();
    }
    
    if (value < _min) _min = value;
    if (value > _max) _max = value;
    _sum += value;
    _count++;
}

// cppcheck-suppress unusedFunction
int16_t SensorStats::getMin() const {
    return (_count > 0) ? _min : 0;
}

// cppcheck-suppress unusedFunction
int16_t SensorStats::getMax() const {
    return (_count > 0) ? _max : 0;
}

// cppcheck-suppress unusedFunction
int16_t SensorStats::getAverage() const {
    return (_count > 0) ? static_cast<int16_t>(_sum / _count) : 0;
}

// cppcheck-suppress unusedFunction
void SensorStats::reset() {
    _min = MIN_INITIAL;
    _max = MAX_INITIAL;
    _sum = 0;
    _count = 0;
}

SimpleLight::SimpleLight(const __FlashStringHelper* name, uint8_t pin)
    : IDevice(name, DeviceType::LightSimple), _pin(pin), _state(false) {
    pinMode(_pin, OUTPUT);
    digitalWrite(_pin, LOW);
    DeviceRegistry::instance().registerDevice(this);
    EventSystem::instance().addListener(this, EventType::ButtonPressed);
}

SimpleLight::~SimpleLight() {
    DeviceRegistry::instance().unregisterDevice(this);
    EventSystem::instance().removeListener(this);
}

// cppcheck-suppress unusedFunction
void SimpleLight::toggle() {
    _state = !_state;
    digitalWrite(_pin, _state ? HIGH : LOW);
    EventSystem::instance().emit(EventType::DeviceStateChanged, this, _state);
}

void SimpleLight::handleEvent(EventType type, IDevice* source, int value) {
    static_cast<void>(type);
    static_cast<void>(source);
    static_cast<void>(value);
}

uint8_t& DimmableLight::brightness_multiplier() {
    static uint8_t multiplier = 100;
    return multiplier;
}

DimmableLight::DimmableLight(const __FlashStringHelper* name, uint8_t pin)
    : SimpleLight(name, pin), _targetBrightness(100), _currentBrightness(0),
      _lastBrightness(100), _lastUpdate(0), _lastMultiplier(100) {
    const_cast<DeviceType&>(type) = DeviceType::LightDimmable;
}

// cppcheck-suppress unusedFunction
void DimmableLight::setBrightness(uint8_t level) {
    _targetBrightness = (level > 100) ? 100 : level;
    if (_state) {
        EventSystem::instance().emit(EventType::DeviceValueChanged, this, _targetBrightness);
    }
}

void DimmableLight::update() {
    uint8_t currentMultiplier = brightness_multiplier();
    if (currentMultiplier != _lastMultiplier) {
        _lastMultiplier = currentMultiplier;
        applyHardware();
    }
    
    uint8_t target = _state ? _targetBrightness : 0;
    
    if (_currentBrightness != target) {
        unsigned long now = millis();
        if (now - _lastUpdate >= MS_PER_STEP) {
            uint8_t steps = static_cast<uint8_t>((now - _lastUpdate) / MS_PER_STEP);
            _lastUpdate = now;
            
            if (_currentBrightness < target) {
                _currentBrightness += min(steps, static_cast<uint8_t>(target - _currentBrightness));
            } else {
                _currentBrightness -= min(steps, static_cast<uint8_t>(_currentBrightness - target));
            }
            applyHardware();
        }
    }
}

// cppcheck-suppress unusedFunction
void DimmableLight::toggle() {
    if (_state) {
        _lastBrightness = _targetBrightness;
        _state = false;
    } else {
        _state = true;
        _targetBrightness = _lastBrightness;
    }
    EventSystem::instance().emit(EventType::DeviceStateChanged, this, _state);
}

// cppcheck-suppress unusedFunction
void DimmableLight::setBrightnessMultiplier(uint8_t multiplier) {
    brightness_multiplier() = (multiplier > 100) ? 100 : multiplier;
}

void DimmableLight::applyHardware() {
    uint16_t effective = (static_cast<uint16_t>(_currentBrightness) * brightness_multiplier()) / 100;
    uint8_t pwmValue = map(effective, 0, 100, PWM_MIN, PWM_MAX);
    uint8_t gammaCorrected = pgm_read_byte(&GAMMA_LUT[pwmValue]);
    analogWrite(_pin, gammaCorrected);
}

RGBLight::RGBLight(const __FlashStringHelper* name, uint8_t pin_r, uint8_t pin_g, uint8_t pin_b)
    : DimmableLight(name, pin_r), _pin_g(pin_g), _pin_b(pin_b), _lastColorUpdate(0) {
    const_cast<DeviceType&>(type) = DeviceType::LightRGB;
    pinMode(_pin_g, OUTPUT);
    pinMode(_pin_b, OUTPUT);
    _targetColor = {255, 255, 255};
    _currentColor = {0, 0, 0};
}

// cppcheck-suppress unusedFunction
void RGBLight::setColor(RGBColor c) {
    _targetColor = c;
    EventSystem::instance().emit(EventType::DeviceValueChanged, this, 0);
}

// cppcheck-suppress unusedFunction
void RGBLight::setRed(uint8_t value) {
    _targetColor.r = value;
    EventSystem::instance().emit(EventType::DeviceValueChanged, this, 0);
}

// cppcheck-suppress unusedFunction
void RGBLight::setGreen(uint8_t value) {
    _targetColor.g = value;
    EventSystem::instance().emit(EventType::DeviceValueChanged, this, 0);
}

// cppcheck-suppress unusedFunction
void RGBLight::setBlue(uint8_t value) {
    _targetColor.b = value;
    EventSystem::instance().emit(EventType::DeviceValueChanged, this, 0);
}

// cppcheck-suppress unusedFunction
void RGBLight::setPreset(RGBPreset preset) {
    RGBColor c;
    memcpy_P(&c, &PRESET_COLORS[static_cast<uint8_t>(preset)], sizeof(RGBColor));
    setColor(c);
}

// cppcheck-suppress unusedFunction
void RGBLight::setBrightness(uint8_t level) {
    DimmableLight::setBrightness(level);
}

void RGBLight::update() {
    DimmableLight::update();
    
    unsigned long now = millis();
    
    if (now - _lastColorUpdate >= MS_PER_STEP) {
        uint8_t steps = static_cast<uint8_t>((now - _lastColorUpdate) / MS_PER_STEP);
        _lastColorUpdate = now;
        
        bool colorChanged = false;
        
        // Red channel fading
        if (_currentColor.r < _targetColor.r) {
            uint8_t delta = min(steps, static_cast<uint8_t>(_targetColor.r - _currentColor.r));
            _currentColor.r += delta;
            colorChanged = true;
        } else if (_currentColor.r > _targetColor.r) {
            uint8_t delta = min(steps, static_cast<uint8_t>(_currentColor.r - _targetColor.r));
            _currentColor.r -= delta;
            colorChanged = true;
        }
        
        // Green channel fading
        if (_currentColor.g < _targetColor.g) {
            uint8_t delta = min(steps, static_cast<uint8_t>(_targetColor.g - _currentColor.g));
            _currentColor.g += delta;
            colorChanged = true;
        } else if (_currentColor.g > _targetColor.g) {
            uint8_t delta = min(steps, static_cast<uint8_t>(_currentColor.g - _targetColor.g));
            _currentColor.g -= delta;
            colorChanged = true;
        }
        
        // Blue channel fading
        if (_currentColor.b < _targetColor.b) {
            uint8_t delta = min(steps, static_cast<uint8_t>(_targetColor.b - _currentColor.b));
            _currentColor.b += delta;
            colorChanged = true;
        } else if (_currentColor.b > _targetColor.b) {
            uint8_t delta = min(steps, static_cast<uint8_t>(_currentColor.b - _targetColor.b));
            _currentColor.b -= delta;
            colorChanged = true;
        }
        
        if (colorChanged) {
            applyColor();
        }
    }
}

// cppcheck-suppress unusedFunction
void RGBLight::toggle() {
    DimmableLight::toggle();
}

void RGBLight::applyColor() {
    uint8_t brightness = currentBrightness();
    uint8_t multiplier = brightness_multiplier();
    
    uint8_t r = (static_cast<uint16_t>(_currentColor.r) * brightness * multiplier) / 10000;
    uint8_t g = (static_cast<uint16_t>(_currentColor.g) * brightness * multiplier) / 10000;
    uint8_t b = (static_cast<uint16_t>(_currentColor.b) * brightness * multiplier) / 10000;
    
    analogWrite(_pin, pgm_read_byte(&GAMMA_LUT[r]));
    analogWrite(_pin_g, pgm_read_byte(&GAMMA_LUT[g]));
    analogWrite(_pin_b, pgm_read_byte(&GAMMA_LUT[b]));
}

TemperatureSensor::TemperatureSensor(const __FlashStringHelper* name)
    : IDevice(name, DeviceType::SensorTemperature), _temperature(0), _lastRead(0) {
    DeviceRegistry::instance().registerDevice(this);
    _lm75.begin();
}

void TemperatureSensor::update() {
    unsigned long now = millis();
    if (now - _lastRead >= UPDATE_INTERVAL_MS) {
        _lastRead = now;
        _temperature = _lm75.getValue();
        _stats.addSample(_temperature);
        EventSystem::instance().emit(EventType::SensorUpdated, this, _temperature);
    }
}

PhotoresistorSensor::PhotoresistorSensor(const __FlashStringHelper* name, uint8_t pin)
    : IDevice(name, DeviceType::SensorLight), _lightLevel(0), _lastRead(0), _photoSensor(pin) {
    DeviceRegistry::instance().registerDevice(this);
}

void PhotoresistorSensor::update() {
    unsigned long now = millis();
    if (now - _lastRead >= UPDATE_INTERVAL_MS) {
        _lastRead = now;
        int newValue = _photoSensor.getValue();
        
        if (abs(newValue - _lightLevel) >= CHANGE_THRESHOLD) {
            _lightLevel = newValue;
            _stats.addSample(static_cast<int16_t>(_lightLevel));
            EventSystem::instance().emit(EventType::SensorUpdated, this, _lightLevel);
        }
    }
}

// cppcheck-suppress unusedFunction
void PhotoresistorSensor::calibrateCurrentAsMin() {
    _photoSensor.setRawMin(_photoSensor.getRaw());
}

// cppcheck-suppress unusedFunction
void PhotoresistorSensor::calibrateCurrentAsMax() {
    _photoSensor.setRawMax(_photoSensor.getRaw());
}

PIRSensorDevice::PIRSensorDevice(const __FlashStringHelper* name, uint8_t pin)
    : IDevice(name, DeviceType::SensorPIR), _motionDetected(false), _lastRead(0), _pirSensor(pin) {
    DeviceRegistry::instance().registerDevice(this);
}

void PIRSensorDevice::update() {
    unsigned long now = millis();
    if (now - _lastRead >= UPDATE_INTERVAL_MS) {
        _lastRead = now;
        bool newState = _pirSensor.getValue();
        
        if (newState != _motionDetected) {
            _motionDetected = newState;
            EventSystem::instance().emit(EventType::SensorUpdated, this, _motionDetected);
        }
    }
}

RamSensorDevice::RamSensorDevice(const __FlashStringHelper* name)
    : IDevice(name, DeviceType::SensorRAM), _freeRam(0), _lastReported(0), _lastRead(0) {
    DeviceRegistry::instance().registerDevice(this);
    _freeRam = _ramSensor.getValue();
    _lastReported = _freeRam;
    _stats.addSample(_freeRam);
}

void RamSensorDevice::update() {
    unsigned long now = millis();
    if (now - _lastRead >= UPDATE_INTERVAL_MS) {
        _lastRead = now;
        _freeRam = _ramSensor.getValue();
        _stats.addSample(_freeRam);
        
        if (abs(_freeRam - _lastReported) >= CHANGE_THRESHOLD) {
            _lastReported = _freeRam;
            EventSystem::instance().emit(EventType::SensorUpdated, this, _freeRam);
        }
    }
}

VccSensorDevice::VccSensorDevice(const __FlashStringHelper* name)
    : IDevice(name, DeviceType::SensorVCC), _vcc(0), _lastRead(0) {
    DeviceRegistry::instance().registerDevice(this);
    _vcc = _vccSensor.getValue();
    _stats.addSample(_vcc);
}

void VccSensorDevice::update() {
    unsigned long now = millis();
    if (now - _lastRead >= UPDATE_INTERVAL_MS) {
        _lastRead = now;
        _vcc = _vccSensor.getValue();
        _stats.addSample(_vcc);
        EventSystem::instance().emit(EventType::SensorUpdated, this, _vcc);
    }
}

LoopTimeSensorDevice::LoopTimeSensorDevice(const __FlashStringHelper* name)
    : IDevice(name, DeviceType::SensorLoopTime), _loopTime(0), _lastRead(0) {
    DeviceRegistry::instance().registerDevice(this);
}

// cppcheck-suppress unusedFunction
void LoopTimeSensorDevice::registerLoopTime(uint16_t microseconds) {
    LoopTimeSensor::registerTime(microseconds);
}

void LoopTimeSensorDevice::update() {
    _loopSensor.updateWindow();
    
    unsigned long now = millis();
    if (now - _lastRead >= UPDATE_INTERVAL_MS) {
        _lastRead = now;
        _loopTime = _loopSensor.getValue();
        _stats.addSample(_loopTime);
        EventSystem::instance().emit(EventType::SensorUpdated, this, _loopTime);
    }
}

OutsideLight::OutsideLight(const __FlashStringHelper* name, uint8_t pin,
                           PhotoresistorSensor* photo, PIRSensorDevice* motion)
    : SimpleLight(name, pin), _mode(OutsideMode::OFF), _photo(photo), _motion(motion) {
    const_cast<DeviceType&>(type) = DeviceType::LightOutside;
    
    if (_photo) {
        EventSystem::instance().addListener(this, EventType::SensorUpdated);
    }
}

OutsideLight::~OutsideLight() {
    EventSystem::instance().removeListener(this);
}

// cppcheck-suppress unusedFunction
void OutsideLight::setMode(OutsideMode mode) {
    _mode = mode;
    evaluateState();
}

// cppcheck-suppress unusedFunction
void OutsideLight::toggle() {
    if (_mode == OutsideMode::OFF) {
        setMode(OutsideMode::ON);
    } else {
        setMode(OutsideMode::OFF);
    }
}

void OutsideLight::evaluateState() {
    bool shouldBeOn = false;
    
    switch (_mode) {
        case OutsideMode::OFF:
            shouldBeOn = false;
            break;
        case OutsideMode::ON:
            shouldBeOn = true;
            break;
        case OutsideMode::AUTO_LIGHT:
            if (_photo) {
                shouldBeOn = (_photo->getValue() < DARKNESS_THRESHOLD);
            }
            break;
        case OutsideMode::AUTO_MOTION:
            if (_photo && _motion) {
                shouldBeOn = (_photo->getValue() < DARKNESS_THRESHOLD) && _motion->isMotionDetected();
            }
            break;
    }
    
    if (shouldBeOn != _state) {
        _state = shouldBeOn;
        digitalWrite(_pin, _state ? HIGH : LOW);
        EventSystem::instance().emit(EventType::DeviceStateChanged, this, _state);
    }
}

void OutsideLight::handleEvent(EventType type, IDevice* source, int value) {
    static_cast<void>(value);
    
    if (type == EventType::SensorUpdated) {
        if (source == _photo || source == _motion) {
            if (_mode == OutsideMode::AUTO_LIGHT || _mode == OutsideMode::AUTO_MOTION) {
                evaluateState();
            }
        }
    }
}

// cppcheck-suppress unusedFunction
void DeviceFactory::createSimpleLight(const __FlashStringHelper* name, uint8_t pin) {
    new SimpleLight(name, pin);
}

// cppcheck-suppress unusedFunction
void DeviceFactory::createDimmableLight(const __FlashStringHelper* name, uint8_t pin) {
    new DimmableLight(name, pin);
}

// cppcheck-suppress unusedFunction
void DeviceFactory::createTemperatureSensor(const __FlashStringHelper* name) {
    new TemperatureSensor(name);
}

// cppcheck-suppress unusedFunction
void DeviceFactory::createRGBLight(const __FlashStringHelper* name, uint8_t r, uint8_t g, uint8_t b) {
    new RGBLight(name, r, g, b);
}

// cppcheck-suppress unusedFunction
OutsideLight* DeviceFactory::createOutsideLight(const __FlashStringHelper* name, uint8_t pin,
                                                PhotoresistorSensor* p, PIRSensorDevice* m) {
    return new OutsideLight(name, pin, p, m);
}

// cppcheck-suppress unusedFunction
PhotoresistorSensor* DeviceFactory::createPhotoresistorSensor(const __FlashStringHelper* name, uint8_t pin) {
    return new PhotoresistorSensor(name, pin);
}

// cppcheck-suppress unusedFunction
PIRSensorDevice* DeviceFactory::createPIRSensor(const __FlashStringHelper* name, uint8_t pin) {
    return new PIRSensorDevice(name, pin);
}

// cppcheck-suppress unusedFunction
void DeviceFactory::createRamSensor(const __FlashStringHelper* name) {
    new RamSensorDevice(name);
}

// cppcheck-suppress unusedFunction
void DeviceFactory::createVoltageSensor(const __FlashStringHelper* name) {
    new VccSensorDevice(name);
}

// cppcheck-suppress unusedFunction
void DeviceFactory::createLoopTimeSensor(const __FlashStringHelper* name) {
    new LoopTimeSensorDevice(name);
}
