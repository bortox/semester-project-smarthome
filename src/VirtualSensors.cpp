/**
 * @file VirtualSensors.cpp
 * @brief Static member definitions for virtual sensors
 * @ingroup Devices
 */
#include "sensors.h"

// Static instance pointer initialization (moved from header to avoid multiple definition)
LoopTimeSensor* LoopTimeSensor::_instance = nullptr;
