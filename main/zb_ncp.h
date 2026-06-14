#pragma once
#include "zboss_decl.h"
#include <stdio.h>
#include <atomic>
#include <sys/time.h>
#include "esp_ota_ops.h"
#include "commands.h"
#include "zb_coordinator_ctx.h"

extern "C" {
    void zb_aib_tcpol_set_update_trust_center_link_keys_required(zb_bool_t);
    void zboss_signal_handler(zb_uint8_t param);
    bool zb_zcl_green_power_cluster_handler(zb_uint8_t param);
    void commissioning_callback(zb_uint8_t param);
    void wakeup_callback(zb_uint8_t param);
    zb_uint8_t data_indication_callback(zb_uint8_t param);
    void zb_zgp_gpdf_raw_indication(zb_bufid_t buf_ref);
    void device_attribute_callback(zb_uint8_t param);
    void firmware_upgrade_callback(zb_uint8_t param);
    zb_bool_t set_real_time_clock(zb_uint32_t time);
    zb_uint8_t zcl_specific_cluster_cmd_handler(zb_uint8_t param);
}

#define OTA_APP_VERSION             PROJECT_VER_UINT32
#define OTA_MANUFACTURER            ESP_ZB_OTA_UPGRADE_MANUFACTURER_CODE_DEF_VALUE
#define OTA_PARTITION_SIZE          1310720
#define OTA_MAX_DATA_SIZE           82
#define OTA_IMAGE_HEADER_LEN        66
#define OTA_IMAGE_TYPE              0x0001 // A custom Device Type = ZB_Coordinator
#define _HW_CHIP_ESP32H2            0x0200
#define _HW_CHIP_ESP32C5            0x0500
#define _HW_CHIP_ESP32C6            0x0600
#define _HW_ANTENNA_DEFAULT         0x0000
#define _HW_ANTENNA_EXTERNAL        0x0010
#define _HW_BUS_UART                0x0001
#define _HW_BUS_USB                 0x0002

#if defined(CONFIG_IDF_TARGET_ESP32C5)
#define _CURRENT_HW_CHIP            _HW_CHIP_ESP32C5
#elif defined(CONFIG_IDF_TARGET_ESP32C6)
#define _CURRENT_HW_CHIP            _HW_CHIP_ESP32C6
#elif defined(CONFIG_IDF_TARGET_ESP32H2)
#define _CURRENT_HW_CHIP            _HW_CHIP_ESP32H2
#else
#error "Unsupported ESP32 chip"
#endif

#if defined(CONFIG_NCP_EXTERNAL_ANTENNA)
#define _CURRENT_HW_ANT             _HW_ANTENNA_EXTERNAL
#else
#define _CURRENT_HW_ANT             _HW_ANTENNA_DEFAULT
#endif

#if defined(CONFIG_NCP_BUS_MODE_UART)
#define _CURRENT_HW_BUS             _HW_BUS_UART
#elif defined(CONFIG_NCP_BUS_MODE_USB)
#define _CURRENT_HW_BUS             _HW_BUS_USB
#else
#error "Unsupported transport"
#endif

/** A custom firmware version depends on the ESP32-board and the connection interface (USB, UART).
 */
#define OTA_HARDWARE_VERSION        (_CURRENT_HW_CHIP | _CURRENT_HW_ANT | _CURRENT_HW_BUS)

#define ZB_APS_FRAME_DATA           0U
#define ZB_APS_FRAME_COMMAND        1U
#define ZB_APS_FRAME_ACK            2U

#define ZB_APS_DELIVERY_UNICAST     0U
#define ZB_APS_DELIVERY_RESERVED    1U
#define ZB_APS_DELIVERY_BROADCAST   2U
#define ZB_APS_DELIVERY_GROUP       3U

#ifndef ZB_APS_FC_GET_FRAME_TYPE
#define ZB_APS_FC_GET_FRAME_TYPE(fc) ((fc) & 3U)
#endif

#ifndef ZB_APS_FC_GET_DELIVERY_MODE
#define ZB_APS_FC_GET_DELIVERY_MODE(fc) (((fc) >> 2U) & 3U)
#endif

#ifndef ZB_APS_FC_GET_ACK_FORMAT
#define ZB_APS_FC_GET_ACK_FORMAT(fc) (((fc) >> 4U) & 1U)
#endif

#ifndef ZB_APS_FC_GET_SECURITY
#define ZB_APS_FC_GET_SECURITY(fc) (((fc)>>5U) & 1U)
#define ZB_APS_FC_IS_SECURE(fc) (ZB_APS_FC_GET_SECURITY(fc) != 0U)
#endif

#ifndef ZB_APS_FC_GET_ACK_REQUEST
#define ZB_APS_FC_GET_ACK_REQUEST(fc) (((fc)>>6U) & 1U)
#endif

#ifndef ZB_APS_FC_GET_EXT_HDR_PRESENT
#define ZB_APS_FC_GET_EXT_HDR_PRESENT(fc) (((fc)>>7U) & 1U)
#endif

class zb_ncp {
public:
    enum frame_type_t : uint8_t {
        REQUEST = 0,
        RESPONSE = 1,
        INDICATION = 2
    };
    struct zb_device_ctx {
        // Basic cluster attributes data
        zb_uint8_t g_attr_basic_zcl_version = ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE;
        zb_uint8_t g_attr_basic_application_version = ZB_ZCL_BASIC_APPLICATION_VERSION_DEFAULT_VALUE;
        zb_uint8_t g_attr_basic_stack_version = ZB_ZCL_BASIC_STACK_VERSION_DEFAULT_VALUE;
        zb_uint8_t g_attr_basic_hw_version = ZB_ZCL_BASIC_HW_VERSION_DEFAULT_VALUE;
        zb_char_t g_attr_basic_manufacturer_name[32] = { 0x09, 'E', 'S', 'P', 'R', 'E', 'S', 'S' , 'I', 'F' };
        zb_char_t g_attr_basic_model_identifier[32] = { 0x0e, 'E', 'S', 'P', '3', '2', '-', 'Z', 'B', 'D', 'o', 'n', 'g', 'l', 'e' };
        zb_char_t g_attr_basic_date_code[16] = ZB_ZCL_BASIC_DATE_CODE_DEFAULT_VALUE;
        zb_char_t g_attr_basic_location_description[16] = ZB_ZCL_BASIC_LOCATION_DESCRIPTION_DEFAULT_VALUE;
        zb_uint8_t g_attr_basic_power_source = ZB_ZCL_BASIC_POWER_SOURCE_DC_SOURCE;
        zb_uint8_t g_attr_basic_physical_environment = ZB_ZCL_BASIC_PHYSICAL_ENVIRONMENT_DEFAULT_VALUE;
        zb_char_t g_attr_sw_build_id[17] = ZB_ZCL_BASIC_SW_BUILD_ID_DEFAULT_VALUE;
        // Groups cluster attributes data
        zb_uint8_t g_attr_groups_name_support = 0;
        // Identify cluster attributes data
        zb_uint16_t g_attr_identify_identify_time = ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE;
        // Scenes cluster attributes data
        zb_uint8_t g_attr_scenes_scene_count = ZB_ZCL_SCENES_SCENE_COUNT_DEFAULT_VALUE;
        zb_uint8_t g_attr_scenes_current_scene = ZB_ZCL_SCENES_CURRENT_SCENE_DEFAULT_VALUE;
        zb_uint16_t g_attr_scenes_current_group = ZB_ZCL_SCENES_CURRENT_GROUP_DEFAULT_VALUE;
        zb_uint8_t g_attr_scenes_scene_valid = ZB_ZCL_SCENES_SCENE_VALID_DEFAULT_VALUE;
        zb_uint8_t g_attr_scenes_name_support = 0;
        // On/Off cluster attributes data
        zb_bool_t g_attr_on_off_on_off = ZB_ZCL_ON_OFF_ON_OFF_DEFAULT_VALUE;
        zb_bool_t g_attr_on_off_global_scene_control = ZB_ZCL_ON_OFF_GLOBAL_SCENE_CONTROL_DEFAULT_VALUE;
        zb_uint16_t g_attr_on_off_on_time = ZB_ZCL_ON_OFF_ON_TIME_DEFAULT_VALUE;
        zb_uint16_t g_attr_on_off_off_wait_time = ZB_ZCL_ON_OFF_OFF_WAIT_TIME_DEFAULT_VALUE;
        // Time cluster attributes data
        zb_uint32_t g_attr_time_time = ZB_ZCL_TIME_TIME_DEFAULT_VALUE;
        zb_int32_t g_attr_time_time_zone = ZB_ZCL_TIME_TIME_ZONE_DEFAULT_VALUE;
        zb_uint32_t g_attr_time_dst_start = ZB_ZCL_TIME_DST_START_DEFAULT_VALUE;
        zb_uint32_t g_attr_time_dst_end = ZB_ZCL_TIME_DST_END_DEFAULT_VALUE;
        zb_uint32_t g_attr_time_dst_shift = ZB_ZCL_TIME_DST_SHIFT_DEFAULT_VALUE;
        zb_uint32_t g_attr_time_standard_time = ZB_ZCL_TIME_STANDARD_TIME_DEFAULT_VALUE;
        zb_uint32_t g_attr_time_local_time = ZB_ZCL_TIME_LOCAL_TIME_DEFAULT_VALUE;
        zb_uint32_t g_attr_time_last_set_time = ZB_ZCL_TIME_LAST_SET_TIME_DEFAULT_VALUE;
        zb_uint32_t g_attr_time_valid_until_time = ZB_ZCL_TIME_VALID_UNTIL_TIME_DEFAULT_VALUE;
        zb_uint8_t g_attr_time_time_status = (1 << ZB_ZCL_TIME_MASTER);
        // OTA Client cluster attributes data
        zb_ieee_addr_t g_attr_ota_client_upgrade_server = ZB_ZCL_OTA_UPGRADE_SERVER_DEF_VALUE;
        zb_uint32_t g_attr_ota_client_file_offset = ZB_ZCL_OTA_UPGRADE_FILE_OFFSET_DEF_VALUE;
        zb_uint32_t g_attr_ota_client_file_version = OTA_APP_VERSION;
        zb_uint32_t g_attr_ota_client_downloaded_file_ver = ZB_ZCL_OTA_UPGRADE_DOWNLOADED_FILE_VERSION_DEF_VALUE;
        zb_uint16_t g_attr_ota_client_stack_version = ZB_ZCL_OTA_UPGRADE_STACK_VERSION_DEF_VALUE;
        zb_uint16_t g_attr_ota_client_downloaded_stack_ver = ZB_ZCL_OTA_UPGRADE_DOWNLOADED_STACK_DEF_VALUE;
        zb_uint16_t g_attr_ota_client_manufacturer = OTA_MANUFACTURER;
        zb_uint16_t g_attr_ota_client_image_type = OTA_IMAGE_TYPE;
        zb_uint16_t g_attr_ota_client_min_block_reque = ESP_ZB_OTA_UPGRADE_MIN_BLOCK_PERIOD_DEF_VALUE;
        zb_uint16_t g_attr_ota_client_image_stamp = ZB_ZCL_OTA_UPGRADE_IMAGE_STAMP_MIN_VALUE; // ESP_ZB_ZCL_OTA_UPGRADE_IMAGE_STAMP_DEF_VALUE;
        zb_uint16_t g_attr_ota_client_server_addr = ZB_ZCL_OTA_UPGRADE_SERVER_ADDR_DEF_VALUE;
        zb_uint8_t g_attr_ota_client_server_ep = ZB_ZCL_OTA_UPGRADE_SERVER_ENDPOINT_DEF_VALUE;
        zb_uint8_t g_attr_ota_client_image_status = ZB_ZCL_OTA_UPGRADE_IMAGE_STATUS_DEF_VALUE;
        // Color Control cluster attributes data
        zb_uint16_t g_attr_color_control_current_X = ZB_ZCL_COLOR_CONTROL_CURRENT_X_DEF_VALUE;
        zb_uint16_t g_attr_color_control_current_Y = ZB_ZCL_COLOR_CONTROL_CURRENT_Y_DEF_VALUE;
    };
    struct cmd_t {
        uint8_t version;
        frame_type_t type;
        command_id_t command_id;
        uint8_t tsn;
    } __attribute__((packed)) __attribute__((aligned(1)));
    zb_ncp(const zb_ncp&) = delete;
    zb_ncp& operator=(const zb_ncp&) = delete;
    zb_ncp(zb_ncp&&) = delete;
    zb_ncp& operator=(zb_ncp&&) = delete;
    static constexpr uint8_t DEVICE_VERSION = 0;
    static constexpr uint8_t MAX_PARALLEL_REQUESTS = 32;
    static inline esp_err_t init() { return instance().init_impl(); }
    static inline esp_err_t deinit() { return instance().deinit_impl(); }
    static void on_rx_data(const void* data, uint16_t size);
    static void send_cmd_data(const void* data, uint16_t size);
    static void indication(command_id_t cmd, const void* data, uint16_t size);
private:
    struct indication_hdr_t {
        uint8_t version;
        frame_type_t type;
        command_id_t command_id;
    } __attribute__((packed)) __attribute__((aligned(1)));
    struct aps_data_ind_header_t {
        uint8_t paramLength;
        uint16_t dataLength;
        uint8_t apsFC;
        uint16_t srcNwk;
        uint16_t dstNwk;
        uint16_t grpNwk;
        uint8_t dstEndpoint;
        uint8_t srcEndpoint;
        uint16_t clusterID;
        uint16_t profileID;
        uint8_t apsCounter;
        uint16_t srcMAC;
        uint16_t dstMAC;
        uint8_t lqi;
        uint8_t rssi;
        uint8_t apsKey;
    } __attribute__((packed)) __attribute__((aligned(1)));
    struct gp_mapping_table_t {
        zb_zgps_mapping_entry_t entry;
        zb_uint8_t payload[128];
        uint32_t sec_frame_counter;
    };
    template <command_id_t CmdId> struct cmd_handle;
    template <command_id_t CmdId, typename Res> struct general_status_res;
    template <command_id_t CmdId, typename Arg> struct general_status_arg;
    template <command_id_t CmdId, typename Arg, typename Res> struct general_status_arg_res;
    template <command_id_t CmdId> struct immediate_cmd_process;
    template <command_id_t CmdId, template<command_id_t> typename ResolveStrategyT> struct delayed_cmd_process;
    template <command_id_t CmdId, typename Arg, typename Req, typename Resp> struct request_cmd_process;
    static constexpr uint16_t ZB_TASK_STACK_SIZE = 16384;
    static constexpr uint16_t TASK_PRIORITY = 16;
    zb_ncp();
    ~zb_ncp();
    static zb_ncp& instance();
    esp_err_t init_impl();
    esp_err_t deinit_impl();
    static void set_channel_mask(uint32_t mask);
    friend void zboss_signal_handler(zb_uint8_t param);
    friend bool zb_zcl_green_power_cluster_handler(zb_uint8_t param);
    friend void commissioning_callback(zb_uint8_t param);
    friend void wakeup_callback(zb_uint8_t param);
    friend zb_uint8_t data_indication_callback(zb_uint8_t param);
    friend void zb_zgp_gpdf_raw_indication(zb_bufid_t buf_ref);
    friend void device_attribute_callback(zb_uint8_t param);
    friend void firmware_upgrade_callback(zb_uint8_t param);
    friend zb_bool_t set_real_time_clock(zb_uint32_t time);
    friend zb_uint8_t zcl_specific_cluster_cmd_handler(zb_uint8_t param);
    void task_impl();
    static inline void task(void* pvParameter) { static_cast<zb_ncp*>(pvParameter)->task_impl(); }
    bool start_zigbee_stack_impl();
    static inline bool start_zigbee_stack() { return instance().start_zigbee_stack_impl(); }
    static esp_err_t sync_mapping_table_from_nvram();
    static int get_gp_device_mapping_table_idx(zb_zgpd_id_t* zgpd_id);
    static bool add_gp_device_to_mapping_table(zb_zgpd_id_t* zgpd_id);
    static bool remove_gp_device_from_mapping_table(zb_zgpd_id_t* zgpd_id);
    static void delete_gp_device(uint8_t* payload, uint16_t payload_len);
    static bool gp_device_commissioning(zb_zgpd_id_t* zgpd_id, zb_uint8_t device_id, zb_uint16_t manuf_id, zb_uint16_t manuf_model_id, zb_ieee_addr_t ieee_addr);
    static bool gp_device_indication(zb_zgpd_id_t* zgpd_id);
    static inline portMUX_TYPE m_mux_lock = portMUX_INITIALIZER_UNLOCKED;
    SemaphoreHandle_t m_stop_sem{ nullptr };
    TaskHandle_t m_task_handle{ nullptr };
    static inline gp_mapping_table_t* d_mapping_table{ nullptr };
    static inline const zb_zgps_mapping_entry_t** d_mapping_table_ptrs{ nullptr };
    static inline zb_uint16_t d_mapping_table_size = 0;
    static inline esp_ota_handle_t m_ota_handle = 0;
    static inline const esp_partition_t* m_update_partition{ nullptr };
    static inline void reset_ota_context() noexcept { if (m_ota_handle != 0) { esp_ota_abort(m_ota_handle); m_ota_handle = 0; } m_update_partition = nullptr; }
};
