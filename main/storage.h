#pragma once

#include <stdio.h>
#include <esp_err.h>
#include "zboss_decl.h"

class storage {
public:
    // New fields can be added for saving to disk
    struct zb_app_config_t {
        uint16_t pan_id = 0;
        uint8_t max_children = 64;
        uint8_t tx_power = 20;
        uint32_t channel_mask = ZB_TRANSCEIVER_ALL_CHANNELS_MASK;
    } __attribute__((packed)) __attribute__((aligned(1)));
    static inline zb_app_config_t& app_config() {
        static zb_app_config_t instance;
        return instance;
    }
    static esp_err_t init();
    static esp_err_t load_config(zb_app_config_t& config);
    static esp_err_t save_config(const zb_app_config_t& config);
    static esp_err_t read_param(size_t offset, void* dst, size_t size);
    static esp_err_t write_param(size_t offset, const void* src, size_t size);
    static esp_err_t delete_config();
    static esp_err_t format();
private:
    static constexpr const char* TAG = "STORAGE";
    static constexpr const char* PARTITION = "storage";
    static constexpr const char* BASE_PATH = "/littlefs";
    static constexpr const char* CONFIG_FILE = "/littlefs/zb_app_config";
    static inline bool _is_mounted = false;
};
