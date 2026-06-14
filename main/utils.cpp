#include "utils.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_phy.h"
#include "statuses.h"

namespace utils {

    void init_external_antenna(const char* TAG) {
#if defined(CONFIG_NCP_EXTERNAL_ANTENNA)
#if CONFIG_NCP_ANTENNA_SWITCH_PIN >= 0
#define ANT_SWITCH_PIN  CONFIG_NCP_ANTENNA_SWITCH_PIN
#elif defined(WIFI_ANT_CONFIG)
#define ANT_SWITCH_PIN  WIFI_ANT_CONFIG
#endif
#if defined(ANT_SWITCH_PIN)
#if defined(CONFIG_IDF_TARGET_ESP32C5)
        esp_phy_ant_gpio_config_t ant_gpio = {};
        ant_gpio.gpio_cfg[0].gpio_select = 1;
        ant_gpio.gpio_cfg[0].gpio_num = static_cast<uint8_t>(ANT_SWITCH_PIN);
        esp_err_t err = esp_phy_set_ant_gpio(&ant_gpio);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set PHY antenna GPIO config (error: %s)", esp_err_to_name(err));
            return;
        }
        esp_phy_ant_config_t ant_config = {
            .rx_ant_mode = ESP_PHY_ANT_MODE_ANT1,
            .rx_ant_default = ESP_PHY_ANT_ANT1,
            .tx_ant_mode = ESP_PHY_ANT_MODE_ANT1,
            .enabled_ant0 = 0,
            .enabled_ant1 = 1
        };
        err = esp_phy_set_ant(&ant_config);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set PHY antenna mode (error: %s)", esp_err_to_name(err));
            return;
        }
        ESP_LOGI(TAG, "PHY Antenna successfully locked to external (ANT1) via GPIO: %d", ANT_SWITCH_PIN);
#else
#if CONFIG_NCP_ANTENNA_CONTROLLER_ENABLE_PIN >= 0
#define ANT_ENABLE_PIN  CONFIG_NCP_ANTENNA_CONTROLLER_ENABLE_PIN
#elif defined(WIFI_ENABLE)
#define ANT_ENABLE_PIN  WIFI_ENABLE
#endif
#if defined(ANT_ENABLE_PIN)
        gpio_config_t io_conf = {};
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pin_bit_mask = (1ULL << ANT_ENABLE_PIN) | (1ULL << ANT_SWITCH_PIN);
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.intr_type = GPIO_INTR_DISABLE;
        esp_err_t err = gpio_config(&io_conf);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to config antenna GPIOs (error: %s)", esp_err_to_name(err));
            return;
        }
        gpio_set_level(static_cast<gpio_num_t>(ANT_ENABLE_PIN), 0);     // Enable RF Switch (LOW)
        gpio_set_level(static_cast<gpio_num_t>(ANT_SWITCH_PIN), 1);     // Select External Antenna (HIGH)
        ESP_LOGI(TAG, "External antenna active. RF Switch pin: %d, Antenna pin: %d", ANT_ENABLE_PIN, ANT_SWITCH_PIN);
#undef ANT_ENABLE_PIN
#else
        ESP_LOGI(TAG, "External antenna requested, but no control hardware pins are defined.");
#endif
#endif
#undef ANT_SWITCH_PIN
#else
        ESP_LOGI(TAG, "External antenna requested, but antenna switch pin is not specified.");
#endif /* ANT_SWITCH_PIN */
#else
        ESP_LOGI(TAG, "Using default antenna.");
#endif /* CONFIG_NCP_EXTERNAL_ANTENNA */
    }

    void reverse_ieee_addr(zb_ieee_addr_t addr) {
        uint64_t val;
        std::memcpy(&val, addr, 8);
        val = __builtin_bswap64(val);
        std::memcpy(addr, &val, 8);
    }

    static const uint8_t crc8_table[] = {
        0xea, 0xd4, 0x96, 0xa8, 0x12, 0x2c, 0x6e, 0x50, 0x7f, 0x41, 0x03, 0x3d, 0x87, 0xb9, 0xfb, 0xc5, 0xa5, 0x9b, 0xd9, 0xe7, 0x5d, 0x63, 0x21, 0x1f,
        0x30, 0x0e, 0x4c, 0x72, 0xc8, 0xf6, 0xb4, 0x8a, 0x74, 0x4a, 0x08, 0x36, 0x8c, 0xb2, 0xf0, 0xce, 0xe1, 0xdf, 0x9d, 0xa3, 0x19, 0x27, 0x65, 0x5b,
        0x3b, 0x05, 0x47, 0x79, 0xc3, 0xfd, 0xbf, 0x81, 0xae, 0x90, 0xd2, 0xec, 0x56, 0x68, 0x2a, 0x14, 0xb3, 0x8d, 0xcf, 0xf1, 0x4b, 0x75, 0x37, 0x09,
        0x26, 0x18, 0x5a, 0x64, 0xde, 0xe0, 0xa2, 0x9c, 0xfc, 0xc2, 0x80, 0xbe, 0x04, 0x3a, 0x78, 0x46, 0x69, 0x57, 0x15, 0x2b, 0x91, 0xaf, 0xed, 0xd3,
        0x2d, 0x13, 0x51, 0x6f, 0xd5, 0xeb, 0xa9, 0x97, 0xb8, 0x86, 0xc4, 0xfa, 0x40, 0x7e, 0x3c, 0x02, 0x62, 0x5c, 0x1e, 0x20, 0x9a, 0xa4, 0xe6, 0xd8,
        0xf7, 0xc9, 0x8b, 0xb5, 0x0f, 0x31, 0x73, 0x4d, 0x58, 0x66, 0x24, 0x1a, 0xa0, 0x9e, 0xdc, 0xe2, 0xcd, 0xf3, 0xb1, 0x8f, 0x35, 0x0b, 0x49, 0x77,
        0x17, 0x29, 0x6b, 0x55, 0xef, 0xd1, 0x93, 0xad, 0x82, 0xbc, 0xfe, 0xc0, 0x7a, 0x44, 0x06, 0x38, 0xc6, 0xf8, 0xba, 0x84, 0x3e, 0x00, 0x42, 0x7c,
        0x53, 0x6d, 0x2f, 0x11, 0xab, 0x95, 0xd7, 0xe9, 0x89, 0xb7, 0xf5, 0xcb, 0x71, 0x4f, 0x0d, 0x33, 0x1c, 0x22, 0x60, 0x5e, 0xe4, 0xda, 0x98, 0xa6,
        0x01, 0x3f, 0x7d, 0x43, 0xf9, 0xc7, 0x85, 0xbb, 0x94, 0xaa, 0xe8, 0xd6, 0x6c, 0x52, 0x10, 0x2e, 0x4e, 0x70, 0x32, 0x0c, 0xb6, 0x88, 0xca, 0xf4,
        0xdb, 0xe5, 0xa7, 0x99, 0x23, 0x1d, 0x5f, 0x61, 0x9f, 0xa1, 0xe3, 0xdd, 0x67, 0x59, 0x1b, 0x25, 0x0a, 0x34, 0x76, 0x48, 0xf2, 0xcc, 0x8e, 0xb0,
        0xd0, 0xee, 0xac, 0x92, 0x28, 0x16, 0x54, 0x6a, 0x45, 0x7b, 0x39, 0x07, 0xbd, 0x83, 0xc1, 0xff,
    };

    static_assert(sizeof(crc8_table) == 256);
    uint8_t crc8(const void* data, uint16_t size) {
        uint8_t crc = 0x00;
        auto bytes = static_cast<const uint8_t*>(data);
        auto end = bytes + size;
        while (bytes != end) {
            crc = crc8_table[crc ^ *bytes++];
        }
        return crc;
    }

    static const uint16_t crc16_table[] = {
        0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf, 0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7, 0x1081, 0x0108,
        0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e, 0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876, 0x2102, 0x308b, 0x0210, 0x1399,
        0x6726, 0x76af, 0x4434, 0x55bd, 0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5, 0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e,
        0x54b5, 0x453c, 0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974, 0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
        0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3, 0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a, 0xdecd, 0xcf44,
        0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72, 0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9, 0xef4e, 0xfec7, 0xcc5c, 0xddd5,
        0xa96a, 0xb8e3, 0x8a78, 0x9bf1, 0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738, 0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862,
        0x9af9, 0x8b70, 0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7, 0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
        0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036, 0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e, 0xa50a, 0xb483,
        0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5, 0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd, 0xb58b, 0xa402, 0x9699, 0x8710,
        0xf3af, 0xe226, 0xd0bd, 0xc134, 0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c, 0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1,
        0xa33a, 0xb2b3, 0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb, 0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
        0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a, 0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1, 0x6b46, 0x7acf,
        0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9, 0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330, 0x7bc7, 0x6a4e, 0x58d5, 0x495c,
        0x3de3, 0x2c6a, 0x1ef1, 0x0f78,
    };

    static_assert(sizeof(crc16_table) == 512);
    uint16_t crc16(const void* data, uint16_t size) {
        uint16_t crc = 0x0000;
        auto bytes = static_cast<const uint8_t*>(data);
        auto end = bytes + size;
        while (bytes != end) {
            crc = crc16_table[(crc ^ *bytes++) & 0xff] ^ ((crc >> 8) & 0xff);
        }
        return crc;
    }

    const char* get_zdp_status_str(uint8_t status) {
        switch (status) {
        case ZB_ZDP_STATUS_SUCCESS:                             return "Success";
        case ZB_ZDP_STATUS_INV_REQUESTTYPE:                     return "Invalid request type";
        case ZB_ZDP_STATUS_DEVICE_NOT_FOUND:                    return "Device not found";
        case ZB_ZDP_STATUS_INVALID_EP:                          return "Invalid endpoint";
        case ZB_ZDP_STATUS_NOT_ACTIVE:                          return "Not active";
        case ZB_ZDP_STATUS_NOT_SUPPORTED:                       return "Not supported";
        case ZB_ZDP_STATUS_TIMEOUT:                             return "Timeout";
        case ZB_ZDP_STATUS_NO_MATCH:                            return "No match";
        case ZB_ZDP_STATUS_NO_ENTRY:                            return "No entry";
        case ZB_ZDP_STATUS_NO_DESCRIPTOR:                       return "No descriptor";
        case ZB_ZDP_STATUS_INSUFFICIENT_SPACE:                  return "Insufficient space";
        case ZB_ZDP_STATUS_NOT_PERMITTED:                       return "Not permitted";
        case ZB_ZDP_STATUS_TABLE_FULL:                          return "Table full";
        case ZB_ZDP_STATUS_NOT_AUTHORIZED:                      return "Not authorized";
        case ZB_ZDP_STATUS_INVALID_INDEX:                       return "Invalid index";
        case ZB_ZDP_STATUS_DEV_ANNCE_SENDING_FAILED:            return "Device announce failed";
        case ZB_ZDP_STATUS_TIMEOUT_BY_STACK:                    return "Stack timeout";
        default:                                                return "Unknown ZDP status";
        }
    }

    const char* get_generic_status_str(uint8_t status) {
        switch (status) {
        case GENERIC_OK:                                        return "Success";
        case GENERIC_ERROR:                                     return "Error";
        case GENERIC_BLOCKED:                                   return "Blocked";
        case GENERIC_EXIT:                                      return "Exit";
        case GENERIC_BUSY:                                      return "Busy";
        case GENERIC_EOF:                                       return "End of file";
        case GENERIC_OUT_OF_RANGE:                              return "Out of range";
        case GENERIC_EMPTY:                                     return "Empty";
        case GENERIC_CANCELLED:                                 return "Cancelled";
        case GENERIC_INVALID_PARAMETER_1:                       return "Invalid parameter 1";
        case GENERIC_INVALID_PARAMETER_2:                       return "Invalid parameter 2";
        case GENERIC_INVALID_PARAMETER_3:                       return "Invalid parameter 3";
        case GENERIC_INVALID_PARAMETER_4:                       return "Invalid parameter 4";
        case GENERIC_INVALID_PARAMETER_5:                       return "Invalid parameter 5";
        case GENERIC_INVALID_PARAMETER_6:                       return "Invalid parameter 6";
        case GENERIC_INVALID_PARAMETER_7:                       return "Invalid parameter 7";
        case GENERIC_INVALID_PARAMETER_8:                       return "Invalid parameter 8";
        case GENERIC_INVALID_PARAMETER_9:                       return "Invalid parameter 9";
        case GENERIC_INVALID_PARAMETER_10:                      return "Invalid parameter 10";
        case GENERIC_INVALID_PARAMETER_11_OR_MORE:              return "Invalid parameter 11 or more";
        case GENERIC_PENDING:                                   return "Pending";
        case GENERIC_NO_MEMORY:                                 return "No memory";
        case GENERIC_INVALID_PARAMETER:                         return "Invalid parameter";
        case GENERIC_OPERATION_FAILED:                          return "Operation failed";
        case GENERIC_BUFFER_TOO_SMALL:                          return "Buffer too small";
        case GENERIC_END_OF_LIST:                               return "End of list";
        case GENERIC_ALREADY_EXISTS:                            return "Already exists";
        case GENERIC_NOT_FOUND:                                 return "Not found";
        case GENERIC_OVERFLOW:                                  return "Overflow";
        case GENERIC_TIMEOUT:                                   return "Timeout";
        case GENERIC_NOT_IMPLEMENTED:                           return "Not implemented";
        case GENERIC_NO_RESOURCES:                              return "No resources";
        case GENERIC_UNINITIALIZED:                             return "Uninitialized";
        case GENERIC_NO_SERVER:                                 return "No server";
        case GENERIC_INVALID_STATE:                             return "Invalid state";
        case GENERIC_CONNECTION_FAILED:                         return "Connection failed";
        case GENERIC_CONNECTION_LOST:                           return "Connection lost";
        case GENERIC_UNAUTHORIZED:                              return "Unauthorized";
        case GENERIC_CONFLICT:                                  return "Conflict";
        case GENERIC_INVALID_FORMAT:                            return "Invalid format";
        case GENERIC_NO_MATCH:                                  return "No match";
        case GENERIC_PROTOCOL_ERROR:                            return "Protocol error";
        case GENERIC_VERSION:                                   return "Version mismatch";
        case GENERIC_MALFORMED_ADDRESS:                         return "Malformed address";
        case GENERIC_COULD_NOT_READ_FILE:                       return "Could not read file";
        case GENERIC_FILE_NOT_FOUND:                            return "File not found";
        case GENERIC_DIRECTORY_NOT_FOUND:                       return "Directory not found";
        case GENERIC_CONVERSION_ERROR:                          return "Conversion error";
        case GENERIC_INCOMPATIBLE_TYPES:                        return "Incompatible types";
        case GENERIC_FILE_CORRUPTED:                            return "File corrupted";
        case GENERIC_PAGE_NOT_FOUND:                            return "Page not found";
        case GENERIC_ILLEGAL_REQUEST:                           return "Illegal request";
        case GENERIC_INVALID_GROUP:                             return "Invalid group";
        case GENERIC_TABLE_FULL:                                return "Table full";
        case GENERIC_IGNORE:                                    return "Ignore";
        case GENERIC_AGAIN:                                     return "Try again";
        case GENERIC_DEVICE_NOT_FOUND:                          return "Device not found";
        case GENERIC_OBSOLETE:                                  return "Obsolete";
        case GENERIC_INTERRUPTED:                               return "Interrupted";
        case GENERIC_NULL_POINTER:                              return "Null pointer";
        default:                                                return "Unknown GENERIC status";
        }
    }

    const char* get_nlme_status_str(uint8_t status) {
        switch (status) {
        case ZB_NWK_COMMAND_STATUS_NO_ROUTE_AVAILABLE:          return "No route available";
        case ZB_NWK_COMMAND_STATUS_TREE_LINK_FAILURE:           return "Tree link failure";
        case ZB_NWK_COMMAND_STATUS_NONE_TREE_LINK_FAILURE:      return "Non-tree link failure";
        case ZB_NWK_COMMAND_STATUS_LOW_BATTERY_LEVEL:           return "Low battery level";
        case ZB_NWK_COMMAND_STATUS_NO_ROUTING_CAPACITY:         return "No routing capacity";
        case ZB_NWK_COMMAND_STATUS_NO_INDIRECT_CAPACITY:        return "No indirect capacity";
        case ZB_NWK_COMMAND_STATUS_INDIRECT_TRANSACTION_EXPIRY: return "Indirect transaction expiry";
        case ZB_NWK_COMMAND_STATUS_TARGET_DEVICE_UNAVAILABLE:   return "Target device unavailable";
        case ZB_NWK_COMMAND_STATUS_TARGET_ADDRESS_UNALLOCATED:  return "Target address unallocated";
        case ZB_NWK_COMMAND_STATUS_PARENT_LINK_FAILURE:         return "Parent link failure";
        case ZB_NWK_COMMAND_STATUS_VALIDATE_ROUTE:              return "Validate route";
        case ZB_NWK_COMMAND_STATUS_SOURCE_ROUTE_FAILURE:        return "Source route failure";
        case ZB_NWK_COMMAND_STATUS_MANY_TO_ONE_ROUTE_FAILURE:   return "Many-to-one route failure";
        case ZB_NWK_COMMAND_STATUS_ADDRESS_CONFLICT:            return "Address conflict";
        case ZB_NWK_COMMAND_STATUS_VERIFY_ADDRESS:              return "Verify address";
        case ZB_NWK_COMMAND_STATUS_PAN_IDENTIFIER_UPDATE:       return "PAN ID update";
        case ZB_NWK_COMMAND_STATUS_NETWORK_ADDRESS_UPDATE:      return "Network address update";
        case ZB_NWK_COMMAND_STATUS_BAD_FRAME_COUNTER:           return "Bad frame counter";
        case ZB_NWK_COMMAND_STATUS_BAD_KEY_SEQUENCE_NUMBER:     return "Bad key sequence number";
        case ZB_NWK_COMMAND_STATUS_UNKNOWN_COMMAND:             return "Unknown command";
        default:                                                return "Unknown NWK status";
        }
    }

}
