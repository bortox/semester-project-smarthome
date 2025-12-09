This projects aims to build a smart home system centered on Object Oriented Programming in C++ language. The Arduino is programmed via AVR with PlatformIO extension on VSCode. We propose a system where it is easy to add a new device to the smart home system; the menus will automatically and dynamically update after updating the configuration. The user can control the smart home system via switches/potentiometers located in the rooms or via a central control system which consists of an Arduino Modulino Knob (Rotary Encoder) and an LCD Display. The LCD Menu is built dynamically leveraging the LCDMenu library by forntoh. The devices are divided in two main categories: Lights and Sensors. Sensors are input devices; in our case we are using an LM75 temperature sensor, a photoresistor and a HC-SR501 PIR movement sensor.

## Lights

There are four different light types.

First there is SimpleLight. A SimpleLight can be turned on or off by the user, and its state can be switched.

Then, a DimmableLight (class inherited from SimpleLight) can also be set to a percent brightness value via a class method, that will be converted internally to PWM signal for the arduino. There is room to implement a fading for a simple light. 

Then, an RGBLight is composed out of three dimmable lights. We can thus drive each channel (R,G,B) as if it was a simple light, we can set an RGBColor from a hardcoded one (WARM_WHITE, OCEAN_BLUE...) or let the user write as if he was setting the intensity of three single DimmableLight objects. 

Ultimately, an AutomaticLight is initialized via a SimpleLight (since it was not explicitely requested in the report to make it dimmable) object, a Movement Sensor and a Photoresistor. Note that, thanks to the code structure, it would be a matter of probably just three rows to add another AutomaticLight on the opposite side of the home and update it

## Sensors

A Sensor is an object with a template function (that can return double, float, int, bool, in short different data types depending on the sensor type) and an obligatory implementation of the getValue() function, and a DATA_UNIT char constant per type of sensor (defaults to none, but can be for example C° for temperature).

## Scenes

The system now includes a **Scene Management System** with priority-based conflict resolution (Painter's Algorithm).

### Scene Types

1. **Night Mode** (Priority 10)
   - Reduces all light brightness to 20% via global multiplier
   - Passive effect applied on enter/exit
   - Can be overridden by higher priority scenes

2. **Party Mode** (Priority 50)
   - Cycles RGB lights through Red → Green → Blue every 500ms
   - Non-blocking animation using millis()
   - Overrides Night Mode, but can be overridden by Alarm

3. **Alarm Mode** (Priority 255 - Maximum)
   - Listens to PIR motion sensor
   - Flashes all RGB lights RED when motion detected
   - Overrides all other scenes
   - Auto-deactivates after 10 seconds of no motion

### Scene Conflict Resolution

Multiple scenes can be active simultaneously. Conflicts are resolved using **priority ordering**:
- SceneManager sorts active scenes by priority (low to high)
- Scenes are applied sequentially, allowing higher priority scenes to overwrite lower ones
- This provides deterministic behavior without complex state merging

Example: If Night Mode (10) and Alarm Mode (255) are both active, Alarm's red flash will override Night Mode's dimming.

## Timer System

A lightweight **enum-based timer system** without std::function overhead.

### Features

- Schedule device actions (TURN_ON, TURN_OFF, TOGGLE, SET_BRIGHTNESS)
- Schedule scene actions (ACTIVATE_SCENE, DEACTIVATE_SCENE)
- Non-blocking execution using millis()
- Automatic task removal after execution
- Memory footprint: ~8 bytes per task

### Usage Example

```cpp
// Turn on light in 30 seconds
TimerManager::instance().addTimer(30000, TimerAction::TURN_ON, myLight);

// Activate party mode in 1 minute
TimerManager::instance().addSceneTimer(60000, TimerAction::ACTIVATE_SCENE, &partyMode);
```

## Menu Builder

Here, the magic happens. We can divide the home in different groups of devices, which will be "Inside" and "Outside". So we will have two main submenus: inside and outside. Then, for every device inside or outside, if it is a sensor we can click on the device name and get more information about the value (for example, we could implement min/max/average value or show a chart). 

If the device is a light, we show the following things:

- ON/OFF
- FLASH
- DIM (only for DimmableLight)

// TO SEE HOW TO IMPLEMENT DIM WHEN WE HAVE A RGB COLOR: DO WE WANT TO PRIORITIZE THE LIGHT OR THE COLOR?

- COLOR (only for RGBLight)

The color menu has

- SET FROM LIST
- CUSTOM COLOR

At the end of every menu, we have the option to go back. 

## Memory consumption results

The system has been heavily optimized for RAM usage on the ATmega328P (2KB total):

- **No floating-point arithmetic**: Temperature stored as decicelsius (int16_t), eliminating float overhead
- **Template-based menu items**: LiveItem<T> replaces bloated InfoItem, reducing code duplication
- **JIT menu allocation**: Pages created on-demand and destroyed when navigating back
- **DynamicArray auto-shrinking**: Automatic memory reclamation when capacity exceeds usage by threshold

Current usage: ~80% RAM with ~400B free after menu creation. The removal of float operations saves approximately 2KB of Flash (library overhead) and reduces stack usage during menu rendering.

