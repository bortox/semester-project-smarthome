/**
 * @file DebugConfig.h
 * @brief Debug configuration flags for the smart home system
 * @author Andrea Bortolotti
 * @version 2.0
 * 
 * @details Centralized debug flag definitions. Enable/disable
 * debug features by setting flags to 1 or 0.
 * 
 * @note This file must remain header-only.
 * 
 * @ingroup Core
 */
#ifndef DEBUG_CONFIG_H
#define DEBUG_CONFIG_H

/**
 * @brief Enable I2C debug LED indicators
 * @details When enabled, LED_BUILTIN flashes during I2C operations
 */
#define DEBUG_I2C 0

/**
 * @brief Enable memory monitoring debug output
 * @details When enabled, free RAM is logged periodically
 */
#define DEBUG_MEMORY 0

/**
 * @brief Enable event system debug output
 * @details When enabled, event emissions are logged
 */
#define DEBUG_EVENTS 0

#endif
