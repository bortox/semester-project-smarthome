# Smart Home Control System

An object-oriented, event-driven firmware for a smart home controller, designed for memory-constrained embedded systems.

**Platform:** Arduino Nano / ATmega328P (2KB SRAM, 32KB Flash)
**Environment:** C++17 with PlatformIO in VSCode

## 📋 Core Features

This project implements a fully-featured smart home controller capable of managing lights, sensors, and automated scenes through a dynamic LCD menu system and physical inputs.

### Supported Hardware
- **Display:** I2C 20x4 LCD
- **Sensors:** LM75 (I2C), Photoresistor (Analog), PIR Motion Sensor (Digital)
- **Actuators:** Simple on/off Lights, Dimmable (PWM) Lights, RGB Lights
- **Inputs:** Physical push-buttons for direct device control and menu navigation, potentiometers for dimming.

### Key Functionalities
- **Dynamic Hierarchical Menu:** The menu is built at runtime based on registered devices, with automatic scrolling and stack-based navigation.
- **Multi-Layer Scene System:** Multiple scenes (e.g., Night Mode, Party, Alarm) can be active simultaneously. Conflicts are resolved deterministically using a priority-based "Painter's Algorithm".
- **Event-Driven Architecture:** The system is fully decoupled. Physical inputs, device state changes, and UI updates are communicated through a central event system (Observer Pattern).
- **Automated Light Control:** The `OutsideLight` is fully automated, reacting to `LightSensor` and `MovementSensor` data based on user-selectable modes (On, Off, Auto-Light, Auto-Motion).
- **Optimized for Embedded:** The entire architecture is designed to minimize RAM and Flash usage, with no dynamic memory allocation in the main loop and extensive use of Flash memory for static data.

---

## 🏗️ Software Architecture

The system follows a clean, layered architecture, promoting separation of concerns and high cohesion within each layer.

```
┌─────────────────────────────────────────┐
│  APPLICATION LAYER (main.cpp)           │
│  - Initializes all systems              │
│  - Orchestrates the main update loop    │
└──────────────┬──────────────────────────┘
               │
┌──────────────▼──────────────────────────┐
│  UI LAYER (FlexibleMenu.h)              │
│  - Manages display, navigation, and UI  │
└──────────────┬──────────────────────────┘
               │
┌──────────────▼──────────────────────────┐
│  AUTOMATION LAYER (Scenes.h)            │
│  - Manages high-level behaviors         │
└──────────────┬──────────────────────────┘
               │
┌──────────────▼──────────────────────────┐
│  DEVICE LAYER (Devices.h)               │
│  - Abstracts individual hardware        │
└──────────────┬──────────────────────────┘
               │
┌──────────────▼──────────────────────────┐
│  INPUT LAYER (PhysicalInput.h)          │
│  - Manages physical user inputs         │
└──────────────┬──────────────────────────┘
               │
┌──────────────▼──────────────────────────┐
│  CORE SYSTEM (CoreSystem.h)             │
│  - Provides foundational patterns       │
└─────────────────────────────────────────┘
```

### Main Loop Execution Flow
The `loop()` function orchestrates the system update in a precise order to ensure responsiveness and correct state management:
1.  **Poll Inputs (`InputManager`):** User actions are read first to ensure minimum latency.
2.  **Update Devices (`DeviceRegistry`):** The base physical state of each device (e.g., dimming transitions) is updated.
3.  **Apply Scenes (`SceneManager`):** Active scenes run in priority order, potentially overriding the base device states.
4.  **Render UI (`NavigationManager`):** The LCD is redrawn only if a state change has occurred, reflecting the final state of the system for the current frame.

---

## 💾 Memory Optimization

With only 2KB of SRAM, memory management is the core design challenge. The following techniques were applied:

- **Just-In-Time (JIT) Menu Allocation:** `MenuPage` objects are created on the heap only when a submenu is entered and are immediately destroyed (`delete`) when the user navigates back. This keeps only the current navigation path in RAM.
- **No Floating-Point Arithmetic:** Temperature is stored as "decicelsius" (`int16_t`, e.g., 23.5°C is stored as 235). All calculations use integer math, avoiding the ~2KB Flash and significant stack overhead of float libraries.
- **Flash Memory for All Static Data:** All device names, menu titles, and labels are stored in program memory using the `F()` macro and `__FlashStringHelper`.
- **Custom `DynamicArray`:** A lightweight vector replacement using `uint8_t` for size/capacity and featuring an auto-shrink mechanism to reclaim unused memory. Copy operations are deleted to prevent memory corruption.
- **Optimized Build Flags:** The `platformio.ini` is configured with `-Os` (optimize for size), `-flto` (Link-Time Optimization), and `-fno-exceptions` to produce the smallest possible binary.

**Memory Budget (2048B Total):**
- **Static & Global Data:** ~500B
- **Heap (Devices + Menu Stack):** ~900B (peak usage in deepest menu)
- **Stack:** ~200B (peak during LCD redraw)
- **Free RAM:** ~400B (average), ~200B (minimum) ✅

---

## 🎨 Design Philosophy & Coding Style

The project is guided by modern C++ best practices, adapted for embedded systems.

### Guiding Principles
1.  **Low Coupling, High Cohesion:** Layers are decoupled via interfaces and the event system. A change in the `PhysicalInput` layer does not require a change in the `UI` layer.
2.  **DRY (Don't Repeat Yourself):** Logic is centralized. `ValueSliderItem` uses templates and member function pointers to work with any device, avoiding code duplication.
3.  **OOP & Polymorphism:** All devices are treated as `IDevice*` and all menu items as `MenuItem*`. This allows core systems to manage heterogeneous collections of objects seamlessly.
4.  **Extensibility:** The architecture is designed for easy expansion. Adding a new sensor requires only implementing the `IDevice` interface and creating a builder function in `MenuBuilder`.

### Naming & Style Conventions
- **Classes & Enums:** `PascalCase` (e.g., `NavigationManager`)
- **Methods & Variables:** `camelCase` (e.g., `addItem`)
- **Private Members:** `_camelCase` with an underscore prefix (e.g., `_selectedIndex`)
- **Constants & Macros:** `UPPER_SNAKE_CASE` (e.g., `DEBOUNCE_DELAY`)
- **Type Safety:** `enum class` is used for strong typing. The `override` keyword is used for virtual methods. `const` is used extensively.

---

## 🔮 Future Ideas

- **Timers:** Develop a `TimerScene` and menu interface to schedule device state changes (e.g., turn off a light in 10 minutes). This would require a new `TimerManager` singleton to handle multiple concurrent timers without blocking.
- **Non-blocking peripherals** Refactor the LCD Driver (and all other future drivers like I2C) to be non blocking using state machines
- **Wireless Control:** Integrate an ESP8266 or similar module to expose a simple API for control via WiFi, adding a new "source" to the `EventSystem` to inject commands from a remote client. The system could be integrated with Home Assistant using MQTT for example, or Zigbee messages from the newer ESP modules. 
- **P2P Communication** Use LoRa to communicate between smart homes for smart home communities (practical example: alarm event sharing), use BLE / Espressif to integrate smart home "modules" into a home using the API built in Wireless Control. 
- **Advanced Sensor Fusion:** Enhance the `SceneManager` to make decisions based on combined sensor inputs (e.g., "if temperature is high AND it's dark, activate 'Cooling Mode' scene").