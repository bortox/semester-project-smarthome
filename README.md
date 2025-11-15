This projects aims to build a smart home system centered on Object Oriented Programming in C++ language. The Arduino is programmed via AVR with PlatformIO extension on VSCode. We propose a system where it is easy to add a new device to the smart home system; the menus will automatically and dynamically update after updating the configuration. The user can control the smart home system via switches/potentiometers located in the rooms or via a central control system which consists of an Arduino Modulino Knob (Rotary Encoder) and an LCD Display. The LCD Menu is built dynamically leveraging the LCDMenu library by forntoh. The devices are divided in two main categories: Lights and Sensors. Sensors are input devices; in our case we are using an LM75 temperature sensor, a photoresistor and a HC-SR501 PIR movement sensor.

## Lights

There are four different light types.

First there is SimpleLight. A SimpleLight can be turned on or off by the user, and its state can be switched.

Then, a DimmableLight (class inherited from SimpleLight) can also be set to a percent brightness value via a class method, that will be converted internally to PWM signal for the arduino. There is room to implement a fading for a simple light. 

Then, an RGBLight is composed out of three dimmable lights. We can thus drive each channel (R,G,B) as if it was a simple light, we can set an RGBColor from a hardcoded one (WARM_WHITE, OCEAN_BLUE...) or let the user write as if he was setting the intensity of three single DimmableLight objects. 

Ultimately, an AutomaticLight is initialized via a SimpleLight (since it was not explicitely requested in the report to make it dimmable) object, a Movement Sensor and a Photoresistor. Note that, thanks to the code structure, it would be a matter of probably just three rows to add another AutomaticLight on the opposite side of the home and update it

## Sensors

A Sensor is an object with a template function (that can return double, float, int, bool, in short different data types depending on the sensor type) and an obligatory implementation of the getValue() function, and a DATA_UNIT char constant per type of sensor (defaults to none, but can be for example C° for temperature).

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

For now, we have used 80% of the RAM and have only 300B free after the menu creation. It will be quite hard to wire also that modulino library. 

