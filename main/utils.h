#pragma once
#include <cstdint>
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "board_pins.h"
#include "zboss_decl.h"

#define IEEE_ADDR_FMT "%02x %02x %02x %02x %02x %02x %02x %02x"
#define IEEE_ADDR_PRINT(a) a[7], a[6], a[5], a[4], a[3], a[2], a[1], a[0]
#define IS_GROUP(a) ((a)[1] == 0 && (a)[2] == 0 && (a)[3] == 0 && (a)[4] == 0 && (a)[5] == 0 && (a)[6] == 0 && (a)[7] == 0)

namespace utils {
    class critical_section {
        portMUX_TYPE* m_mux;
    public:
        critical_section(portMUX_TYPE* mux) : m_mux(mux) {
#if (configNUM_CORES > 1)
            vPortEnterCriticalSafe(*m_mux);
#else
            portENTER_CRITICAL_SAFE(*m_mux);
#endif
        }
        ~critical_section() {
#if (configNUM_CORES > 1)
            vPortExitCriticalSafe(*m_mux);
#else
            portEXIT_CRITICAL_SAFE(*m_mux);
#endif
        }
        critical_section(const critical_section&) = delete;
        critical_section& operator=(const critical_section&) = delete;
    };
    void init_external_antenna(const char* TAG);
    void reverse_ieee_addr(zb_ieee_addr_t addr);
    uint8_t crc8(const void* data, uint16_t size);
    uint16_t crc16(const void* data, uint16_t size);
    const char* get_zdp_status_str(uint8_t status);
    const char* get_generic_status_str(uint8_t status);
    const char* get_nlme_status_str(uint8_t status);
}
