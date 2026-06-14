#ifndef Board_Pins_h
#define Board_Pins_h

#include <stdint.h>

#if defined(CONFIG_IDF_TARGET_ESP32C5)
static constexpr uint8_t TX = 11;
static constexpr uint8_t RX = 12;
#define WIFI_ANT_CONFIG       26
#elif defined(CONFIG_IDF_TARGET_ESP32C6)
static constexpr uint8_t TX = 16;
static constexpr uint8_t RX = 17;
#define WIFI_ENABLE           3
#define WIFI_ANT_CONFIG       14
#elif defined(CONFIG_IDF_TARGET_ESP32H2)
static constexpr uint8_t TX = 24;
static constexpr uint8_t RX = 23;
#else
#error "Unsupported ESP32 chip"
#endif

#endif /* Board_Pins_h */
