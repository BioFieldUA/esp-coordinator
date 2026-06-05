#pragma once

#include "zboss_api_zgp.h"

/** @struct Green Power Device Indication Buffer
 */
struct green_power_00_zb_buf_t {
    zb_uint16_t options;
    zb_uint32_t src_id;             // ZGPD SrcId @see ZGP spec, A.1.4.1.4
    zb_uint32_t sec_frame_counter;  // Security frame counter
    zb_uint8_t zgpd_cmd_id;         // ZGPD command ID
    zb_uint8_t payload_len;         // Length of the GPDF NWK header
} __attribute__((packed)) __attribute__((aligned(1)));

struct green_power_10_zb_buf_t {
    zb_uint16_t options;
    zb_ieee_addr_t ieee_addr;       // ZGPD ieee_addr
    zb_uint8_t endpoint;            // ZGPD Endpoint
    zb_uint32_t sec_frame_counter;  // Security frame counter
    zb_uint8_t zgpd_cmd_id;         // ZGPD command ID
    zb_uint8_t payload_len;         // Length of the GPDF NWK header
} __attribute__((packed)) __attribute__((aligned(1)));

struct green_power_01_zb_buf_t {
    zb_uint16_t options;
    zb_ieee_addr_t ieee_addr;       // ZGPD ieee_addr
    zb_uint32_t sec_frame_counter;  // Security frame counter
    zb_uint8_t zgpd_cmd_id;         // ZGPD command ID
    zb_uint8_t payload_len;         // Length of the GPDF NWK header
} __attribute__((packed)) __attribute__((aligned(1)));

class GreenPowerParser {
private:
    template <typename F>
    inline auto execute(F&& func) const {
        if (m_app_id == ZB_ZGP_APP_ID_0000) {
            return func(reinterpret_cast<green_power_00_zb_buf_t*>(m_raw_ptr));
        } else if (m_app_id == ZB_ZGP_APP_ID_0010) {
            return func(reinterpret_cast<green_power_10_zb_buf_t*>(m_raw_ptr));
        } else {
            return func(reinterpret_cast<green_power_01_zb_buf_t*>(m_raw_ptr));
        }
    }
    template <size_t A, size_t B, size_t C>
    struct static_max {
        static constexpr size_t val1 = (A > B) ? A : B;
        static constexpr uint8_t value = static_cast<uint8_t>((val1 > C) ? val1 : C);
    };
    uint8_t m_app_id;
    void* m_raw_ptr;
public:
    static inline constexpr uint8_t MAX_SIZE = static_max<sizeof(green_power_00_zb_buf_t), sizeof(green_power_10_zb_buf_t), sizeof(green_power_01_zb_buf_t)>::value;
    inline GreenPowerParser(uint8_t app_id, void* raw_ptr) noexcept
        : m_app_id(app_id), m_raw_ptr(raw_ptr) {
    }
    inline uint8_t size() const noexcept {
        return execute([](auto* buf) -> uint8_t {
            return sizeof(*buf);
        });
    }
    inline uint8_t* get_ptr() noexcept {
        return reinterpret_cast<uint8_t*>(m_raw_ptr);
    }
    inline const uint8_t* get_ptr() const noexcept {
        return reinterpret_cast<const uint8_t*>(m_raw_ptr);
    }
    inline uint8_t* get_end_ptr() noexcept {
        return reinterpret_cast<uint8_t*>(m_raw_ptr) + size();
    }
    inline const uint8_t* get_end_ptr() const noexcept {
        return reinterpret_cast<const uint8_t*>(m_raw_ptr) + size();
    }
    inline uint16_t get_options() const noexcept {
        return execute([](auto* buf) { return buf->options; });
    }
    inline void set_options(uint16_t val) noexcept {
        execute([val](auto* buf) { buf->options = val; });
    }
    inline uint32_t get_sec_frame_counter() const noexcept {
        return execute([](auto* buf) { return buf->sec_frame_counter; });
    }
    inline void set_sec_frame_counter(uint32_t val) noexcept {
        execute([val](auto* buf) { buf->sec_frame_counter = val; });
    }
    inline uint8_t get_zgpd_cmd_id() const noexcept {
        return execute([](auto* buf) { return buf->zgpd_cmd_id; });
    }
    inline void set_zgpd_cmd_id(uint8_t val) noexcept {
        execute([val](auto* buf) { buf->zgpd_cmd_id = val; });
    }
    inline uint8_t get_payload_len() const noexcept {
        return execute([](auto* buf) { return buf->payload_len; });
    }
    inline void set_payload_len(uint8_t val) noexcept {
        execute([val](auto* buf) { buf->payload_len = val; });
    }
    inline uint32_t get_src_id() const noexcept {
        if (m_app_id != ZB_ZGP_APP_ID_0000) return 0;
        return reinterpret_cast<green_power_00_zb_buf_t*>(m_raw_ptr)->src_id;
    }
    inline void set_src_id(uint32_t val) noexcept {
        if (m_app_id == ZB_ZGP_APP_ID_0000) {
            reinterpret_cast<green_power_00_zb_buf_t*>(m_raw_ptr)->src_id = val;
        }
    }
    inline uint8_t get_endpoint() const noexcept {
        if (m_app_id != ZB_ZGP_APP_ID_0010) return 0;
        return reinterpret_cast<green_power_10_zb_buf_t*>(m_raw_ptr)->endpoint;
    }
    inline void set_endpoint(uint8_t val) noexcept {
        if (m_app_id == ZB_ZGP_APP_ID_0010) {
            reinterpret_cast<green_power_10_zb_buf_t*>(m_raw_ptr)->endpoint = val;
        }
    }
    inline zb_ieee_addr_t* get_ieee_addr() noexcept {
        if (m_app_id == ZB_ZGP_APP_ID_0000) return nullptr;
        if (m_app_id == ZB_ZGP_APP_ID_0010) {
            return reinterpret_cast<zb_ieee_addr_t*>(reinterpret_cast<green_power_10_zb_buf_t*>(m_raw_ptr)->ieee_addr);
        } else {
            return reinterpret_cast<zb_ieee_addr_t*>(reinterpret_cast<green_power_01_zb_buf_t*>(m_raw_ptr)->ieee_addr);
        }
    }
    inline void set_ieee_addr(const zb_ieee_addr_t src_ieee) noexcept {
        if (m_app_id == ZB_ZGP_APP_ID_0000) return;
        uint8_t* dest = nullptr;
        if (m_app_id == ZB_ZGP_APP_ID_0010) {
            dest = reinterpret_cast<green_power_10_zb_buf_t*>(m_raw_ptr)->ieee_addr;
        } else {
            dest = reinterpret_cast<green_power_01_zb_buf_t*>(m_raw_ptr)->ieee_addr;
        }
        memcpy(dest, src_ieee, sizeof(zb_ieee_addr_t));
    }
    inline zb_zgpd_id_t get_zgpd_id() noexcept {
        if (m_app_id == ZB_ZGP_APP_ID_0000) {
            return zb_zgpd_id_t{
                .app_id = ZB_ZGP_APP_ID_0000,
                .endpoint = get_endpoint(),
                .addr = {
                    .src_id = get_src_id()
                }
            };
        }
        zb_zgpd_id_t id{
            .app_id = m_app_id,
            .endpoint = get_endpoint(),
            .addr = {}
        };
        memcpy(id.addr.ieee_addr, get_ieee_addr(), sizeof(zb_ieee_addr_t));
        return id;
    }
};
