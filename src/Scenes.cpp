/**
 * @file Scenes.cpp
 * @brief Scene management system implementation
 * @author Andrea Bortolotti
 * @version 2.0
 * @ingroup Automation
 * 
 * Implements scene lifecycle management and concrete scene behaviors:
 * - NightModeScene: Global brightness reduction
 * - PartyScene: RGB color cycling animation
 * - AlarmScene: Motion-triggered emergency flashing
 */

#include "Scenes.h"
#include "Devices.h"

// ==================== IScene ====================

IScene::IScene(uint8_t priority) 
    : _active(false), _priority(priority) {}

bool IScene::isActive() const { 
    return _active; 
}

uint8_t IScene::getPriority() const { 
    return _priority; 
}

void IScene::setActive(bool active) {
    if (active && !_active) {
        _active = true;
        onActivate();
    } else if (!active && _active) {
        onDeactivate();
        _active = false;
    }
}

// ==================== SceneManager ====================

SceneManager& SceneManager::instance() {
    static SceneManager inst;
    return inst;
}

bool SceneManager::addScene(IScene* scene) {
    if (!scene) return false;
    
    for (uint8_t i = 0; i < _activeScenes.size(); i++) {
        if (_activeScenes[i] == scene) return false;
    }
    
    scene->setActive(true);
    return _activeScenes.add(scene);
}

void SceneManager::removeScene(IScene* scene) {
    if (!scene) return;
    
    for (uint8_t i = 0; i < _activeScenes.size(); i++) {
        if (_activeScenes[i] == scene) {
            scene->setActive(false);
            _activeScenes.remove(i);
            return;
        }
    }
}

void SceneManager::clearAll() {
    for (uint8_t i = 0; i < _activeScenes.size(); i++) {
        _activeScenes[i]->setActive(false);
    }
    _activeScenes.clear();
}

void SceneManager::update() {
    uint8_t count = _activeScenes.size();
    if (count == 0) return;
    
    for (uint8_t i = 0; i < count - 1; i++) {
        for (uint8_t j = 0; j < count - i - 1; j++) {
            if (_activeScenes[j]->getPriority() > _activeScenes[j + 1]->getPriority()) {
                IScene* temp = _activeScenes[j];
                _activeScenes[j] = _activeScenes[j + 1];
                _activeScenes[j + 1] = temp;
            }
        }
    }
    
    for (uint8_t i = 0; i < count; i++) {
        _activeScenes[i]->update();
    }
}

uint8_t SceneManager::getActiveCount() const {
    return _activeScenes.size();
}

// ==================== NightModeScene ====================

NightModeScene::NightModeScene() 
    : IScene(10), _savedMultiplier(100) {}

const char* NightModeScene::getName() const { 
    return "Night Mode"; 
}

void NightModeScene::onActivate() {
    _savedMultiplier = 100;
    DimmableLight::setBrightnessMultiplier(20);
}

void NightModeScene::onDeactivate() {
    DimmableLight::setBrightnessMultiplier(_savedMultiplier);
}

void NightModeScene::update() {
}

// ==================== PartyScene ====================

PartyScene::PartyScene() 
    : IScene(50), _lastChange(0), _colorIndex(0) {}

const char* PartyScene::getName() const { 
    return "Party Mode"; 
}

void PartyScene::onActivate() {
    _lastChange = millis();
    _colorIndex = 0;
}

void PartyScene::onDeactivate() {
}

void PartyScene::update() {
    if (!_active) return;
    
    unsigned long now = millis();
    if (now - _lastChange >= CHANGE_INTERVAL_MS) {
        _lastChange = now;
        _colorIndex = (_colorIndex + 1) % 3;
        
        DeviceRegistry& registry = DeviceRegistry::instance();
        DynamicArray<IDevice*>& devices = registry.getDevices();
        
        for (uint8_t i = 0; i < devices.size(); i++) {
            if (devices[i]->type == DeviceType::LightRGB) {
                RGBLight* rgb = static_cast<RGBLight*>(devices[i]);
                
                RGBColor color;
                switch (_colorIndex) {
                    case 0: color = {255, 0, 0}; break;
                    case 1: color = {0, 255, 0}; break;
                    case 2: color = {0, 0, 255}; break;
                    default: color = {255, 0, 0}; break;
                }
                rgb->setColor(color);
                
                if (rgb->getBrightness() == 0) {
                    rgb->setBrightness(100);
                }
            }
        }
    }
}

// ==================== AlarmScene ====================

AlarmScene::AlarmScene() 
    : IScene(255), _triggered(false), _flashState(false), 
      _lastFlash(0), _lastMotion(0) {}

const char* AlarmScene::getName() const { 
    return "Alarm Mode"; 
}

void AlarmScene::onActivate() {
    _triggered = false;
    _flashState = false;
    _lastFlash = millis();
    _lastMotion = millis();
    
    EventSystem::instance().addListener(this, EventType::SensorUpdated);
}

void AlarmScene::onDeactivate() {
    EventSystem::instance().removeListener(this);
    
    DeviceRegistry& registry = DeviceRegistry::instance();
    DynamicArray<IDevice*>& devices = registry.getDevices();
    
    for (uint8_t i = 0; i < devices.size(); i++) {
        if (devices[i]->type == DeviceType::LightRGB) {
            RGBLight* rgb = static_cast<RGBLight*>(devices[i]);
            rgb->setBrightness(0);
        }
    }
}

void AlarmScene::handleEvent(EventType type, IDevice* device, int value) {
    if (type == EventType::SensorUpdated && device->type == DeviceType::SensorPIR) {
        if (value == 1) {
            _triggered = true;
            _lastMotion = millis();
        }
    }
}

void AlarmScene::update() {
    if (!_active) return;
    
    unsigned long now = millis();
    
    if (_triggered && (now - _lastMotion > TIMEOUT_MS)) {
        _triggered = false;
        _flashState = false;
        
        DeviceRegistry& registry = DeviceRegistry::instance();
        DynamicArray<IDevice*>& devices = registry.getDevices();
        
        for (uint8_t i = 0; i < devices.size(); i++) {
            if (devices[i]->type == DeviceType::LightRGB) {
                RGBLight* rgb = static_cast<RGBLight*>(devices[i]);
                rgb->setBrightness(0);
            }
        }
    }
    
    if (_triggered && (now - _lastFlash >= FLASH_INTERVAL_MS)) {
        _lastFlash = now;
        _flashState = !_flashState;
        
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
