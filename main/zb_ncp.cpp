#include "zb_ncp.h"
#include "statuses.h"
#include "protocol.h"
#include "storage.h"
#include "zb_zgp_parser.h"
#include "utils.h"

static const char* TAG = "ZBOSS";
#include "commands_impl.h"

/* Zigbee device application context storage. */
static struct zb_ncp::zb_device_ctx dev_ctx;

ZB_ZCL_DECLARE_BASIC_ATTRIB_LIST_EXT(basic_attr_list, &dev_ctx.g_attr_basic_zcl_version, &dev_ctx.g_attr_basic_application_version, &dev_ctx.g_attr_basic_stack_version, &dev_ctx.g_attr_basic_hw_version, &dev_ctx.g_attr_basic_manufacturer_name, &dev_ctx.g_attr_basic_model_identifier, &dev_ctx.g_attr_basic_date_code, &dev_ctx.g_attr_basic_power_source, &dev_ctx.g_attr_basic_location_description, &dev_ctx.g_attr_basic_physical_environment, &dev_ctx.g_attr_sw_build_id);

ZB_ZCL_DECLARE_IDENTIFY_ATTRIB_LIST(identify_attr_list, &dev_ctx.g_attr_identify_identify_time);

ZB_ZCL_DECLARE_GROUPS_ATTRIB_LIST(groups_attr_list, &dev_ctx.g_attr_groups_name_support);

ZB_ZCL_DECLARE_SCENES_ATTRIB_LIST(scenes_attr_list, &dev_ctx.g_attr_scenes_scene_count, &dev_ctx.g_attr_scenes_current_scene, &dev_ctx.g_attr_scenes_current_group, &dev_ctx.g_attr_scenes_scene_valid, &dev_ctx.g_attr_scenes_name_support);

ZB_ZCL_DECLARE_ON_OFF_ATTRIB_LIST_EXT(on_off_attr_list, &dev_ctx.g_attr_on_off_on_off, &dev_ctx.g_attr_on_off_global_scene_control, &dev_ctx.g_attr_on_off_on_time, &dev_ctx.g_attr_on_off_off_wait_time);

ZB_ZCL_DECLARE_TIME_ATTRIB_LIST(time_attr_list, &dev_ctx.g_attr_time_time, &dev_ctx.g_attr_time_time_status, &dev_ctx.g_attr_time_time_zone, &dev_ctx.g_attr_time_dst_start, &dev_ctx.g_attr_time_dst_end, &dev_ctx.g_attr_time_dst_shift, &dev_ctx.g_attr_time_standard_time, &dev_ctx.g_attr_time_local_time, &dev_ctx.g_attr_time_last_set_time, &dev_ctx.g_attr_time_valid_until_time);

ZB_ZCL_DECLARE_OTA_UPGRADE_ATTRIB_LIST(ota_client_attr_list, &dev_ctx.g_attr_ota_client_upgrade_server, &dev_ctx.g_attr_ota_client_file_offset, &dev_ctx.g_attr_ota_client_file_version, &dev_ctx.g_attr_ota_client_stack_version, &dev_ctx.g_attr_ota_client_downloaded_file_ver, &dev_ctx.g_attr_ota_client_downloaded_stack_ver, &dev_ctx.g_attr_ota_client_image_status, &dev_ctx.g_attr_ota_client_manufacturer, &dev_ctx.g_attr_ota_client_image_type, &dev_ctx.g_attr_ota_client_min_block_reque, &dev_ctx.g_attr_ota_client_image_stamp, &dev_ctx.g_attr_ota_client_server_addr, &dev_ctx.g_attr_ota_client_server_ep, OTA_HARDWARE_VERSION, OTA_MAX_DATA_SIZE, ESP_ZB_ZCL_OTA_UPGRADE_QUERY_TIMER_COUNT_DEF);

ZB_ZCL_DECLARE_COLOR_CONTROL_ATTRIB_LIST(color_control_attr_list, &dev_ctx.g_attr_color_control_current_X, &dev_ctx.g_attr_color_control_current_Y);

ZB_DECLARE_COORDINATOR_CLUSTER_LIST(nwk_coordinator_clusters, basic_attr_list, identify_attr_list, groups_attr_list, scenes_attr_list, on_off_attr_list, time_attr_list, ota_client_attr_list, color_control_attr_list);

ZB_DECLARE_COORDINATOR_EP(nwk_coordinator_ep, COORDINATOR_ENDPOINT, nwk_coordinator_clusters);

ZBOSS_DECLARE_DEVICE_CTX_1_EP(nwk_coordinator, nwk_coordinator_ep);

zb_ncp& zb_ncp::instance() {
    static zb_ncp s_zb_ncp;
    return s_zb_ncp;
}

zb_ncp::zb_ncp() {
    m_stop_sem = xSemaphoreCreateBinary();
    if (!m_stop_sem) {
        ESP_LOGE(TAG, "Initialization Failed, error: Stop Semaphore is not created properly");
    }
}

zb_ncp::~zb_ncp() {
    deinit_impl();
    d_mapping_table_size = 0;
    if (d_mapping_table_ptrs) {
        free(d_mapping_table_ptrs);
        d_mapping_table_ptrs = nullptr;
    }
    if (d_mapping_table) {
        free(d_mapping_table);
        d_mapping_table = nullptr;
    }
    reset_ota_context();
    if (m_stop_sem) {
        vSemaphoreDelete(m_stop_sem);
        m_stop_sem = nullptr;
    }
}

esp_err_t zb_ncp::init_impl() {
    if (!m_stop_sem) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(m_stop_sem, 0);
    ESP_LOGI(TAG, "Starting...");
    esp_err_t err = storage::load_config(storage::app_config());
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load Config File (error: %s)", esp_err_to_name(err));
    }
    ZB_INIT("ZIGBEE");
    zb_set_nvram_erase_at_start(ZB_FALSE);
    zb_uint32_t func = ZGP_GPSB_FUNCTIONALITY;
    func |= ZGP_GPS_TRANSLATION_TABLE;
    func |= ZGP_GPS_SEC_LEVEL_FULL_WITH_ENC;
    zb_zgp_set_sink_functionality(func, func);
    func = ZGP_GPPB_FUNCTIONALITY;
    func |= ZGP_GPP_SEC_LEVEL_FULL_WITH_ENC;
    zb_zgp_set_proxy_functionality(func, func);
    ZB_AF_REGISTER_DEVICE_CTX(&nwk_coordinator);
    // ZB_AF_SET_ENDPOINT_HANDLER(COORDINATOR_ENDPOINT, zcl_specific_cluster_cmd_handler);
    // ZB_ZCL_SET_REPORT_ATTR_CB
    // ZB_ZCL_SET_NO_REPORTING_CB
    // ZB_ZCL_SET_DEFAULT_VALUE_CB
    ZB_ZCL_REGISTER_DEVICE_CB(device_attribute_callback);
    zb_set_network_coordinator_role(storage::app_config().channel_mask);
    zb_set_channel_mask(storage::app_config().channel_mask);
    zb_set_bdb_primary_channel_set(storage::app_config().channel_mask);
    zb_set_bdb_secondary_channel_set(storage::app_config().channel_mask);
    zb_set_max_children(storage::app_config().max_children);
    zb_set_pan_id(storage::app_config().pan_id);
    zb_ret_t ret = zb_set_tx_power(storage::app_config().tx_power);
    if (ret != RET_OK) {
        ESP_LOGE(TAG, "Failed to set TX Power (error: %d)", ERROR_GET_CODE(ret));
        return ESP_FAIL;
    }
    ret = zboss_start_no_autostart();
    if (ret != RET_OK) {
        ESP_LOGE(TAG, "Failed to run zboss_start_no_autostart() (error: %d)", ERROR_GET_CODE(ret));
        return ESP_FAIL;
    }
    zb_ieee_addr_t ieee_addr;
    err = esp_read_mac(ieee_addr, ESP_MAC_IEEE802154);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read MAC address, error: %s", esp_err_to_name(err));
        return err;
    }
    utils::reverse_ieee_addr(ieee_addr);
    zb_set_long_address(ieee_addr);
    zb_zgps_set_communication_mode(ZGP_COMMUNICATION_MODE_LIGHTWEIGHT_UNICAST);
    zb_zgps_set_security_level(ZB_ZGP_FILL_GPS_SECURITY_LEVEL(
        ZB_ZGP_SEC_LEVEL_FULL_WITH_ENC,
        ZB_ZGP_SEC_LEVEL_PROTECTION_WITH_GP_LINK_KEY,
        ZB_ZGP_SEC_LEVEL_PROTECTION_DO_NOT_INVOLVE_TC
    ));
    err = sync_mapping_table_from_nvram();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Read Mapping Table from NVRAM, error: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "Maximum number of connected Devices: %d, TX Power: %d, PanID: 0x%04hx, Channel Mask: %08lx", storage::app_config().max_children, storage::app_config().tx_power, zb_get_pan_id(), storage::app_config().channel_mask);
    return protocol::send_wrong_ack();
}

esp_err_t zb_ncp::deinit_impl() {
    if (m_task_handle && m_stop_sem) {
        xTaskNotifyGive(m_task_handle);
        {
            utils::critical_section lock(&zb_ncp::m_mux_lock);
            ZB_SCHEDULE_APP_ALARM(wakeup_callback, 0, 0);
        }
        if (xSemaphoreTake(m_stop_sem, pdMS_TO_TICKS(2000)) == pdTRUE) {
            ESP_LOGI(TAG, "Task deleted");
        } else {
            ESP_LOGE(TAG, "Task hang! Forced deletion.");
            vTaskDelete(m_task_handle);
        }
        zb_set_bdb_secondary_channel_set(0);
        zb_set_bdb_primary_channel_set(0);
        zb_set_channel_mask(0);
    }
    m_task_handle = nullptr;
    return ESP_OK;
}

void zb_ncp::set_channel_mask(uint32_t mask) {
    if (storage::app_config().channel_mask != mask) {
        zb_set_channel_mask(mask);
        zb_set_bdb_primary_channel_set(mask);
        zb_set_bdb_secondary_channel_set(mask);
        esp_err_t ret = storage::write_param(offsetof(storage::zb_app_config_t, channel_mask), &mask, sizeof(mask));
        if (ret == ESP_OK) {
            storage::app_config().channel_mask = mask;
        } else {
            ESP_LOGE(TAG, "Failed to write CHANNEL_MASK to LittleFS Partition (error: %s)", esp_err_to_name(ret));
        }
    }
}

void zb_ncp::task_impl() {
    ESP_LOGI(TAG, "Task Started");
    while (ulTaskNotifyTake(pdTRUE, 0) == 0) {
        zboss_main_loop_iteration();
    }
    ESP_LOGI(TAG, "Task Stoped");
    if (m_stop_sem) xSemaphoreGive(m_stop_sem);
    vTaskDelete(NULL);
}

bool zb_ncp::start_zigbee_stack_impl() {
    if (m_task_handle == nullptr && m_stop_sem) {
        if (xTaskCreate(&task, "zboss", ZB_TASK_STACK_SIZE, this, TASK_PRIORITY, &m_task_handle) == pdTRUE) {
            return true;
        }
    }
    return false;
}

esp_err_t zb_ncp::sync_mapping_table_from_nvram() {
    d_mapping_table_size = 0;
    zb_zgp_ent_enumerate_ctx_t enum_ctx = {
        .idx = ZB_ZGP_ENT_ENUMERATE_CTX_START_IDX,
        .entries_count = 0
    };
    zgp_tbl_ent_t db_entry;
    zb_uint16_t actual_count = 0;
    while (zgp_sink_table_enumerate(&enum_ctx, &db_entry) == RET_OK) {
        actual_count++;
    }
    if (actual_count == 0) {
        d_mapping_table_ptrs = static_cast<const zb_zgps_mapping_entry_t**>(malloc(sizeof(zb_zgps_mapping_entry_t*)));
        if (!d_mapping_table_ptrs) {
            return ESP_ERR_NO_MEM;
        }
        d_mapping_table_ptrs[0] = nullptr;
        ZB_ZGP_SET_MAPPING_TABLE(d_mapping_table_ptrs, &d_mapping_table_size);
        return ESP_OK;
    }
    d_mapping_table = static_cast<gp_mapping_table_t*>(calloc(actual_count, sizeof(gp_mapping_table_t)));
    if (!d_mapping_table) {
        return ESP_ERR_NO_MEM;
    }
    d_mapping_table_ptrs = static_cast<const zb_zgps_mapping_entry_t**>(malloc(actual_count * sizeof(zb_zgps_mapping_entry_t*)));
    if (!d_mapping_table_ptrs) {
        free(d_mapping_table);
        d_mapping_table = nullptr;
        return ESP_ERR_NO_MEM;
    }
    enum_ctx.idx = ZB_ZGP_ENT_ENUMERATE_CTX_START_IDX;
    enum_ctx.entries_count = 0;
    while (zgp_sink_table_enumerate(&enum_ctx, &db_entry) == RET_OK && d_mapping_table_size < actual_count) {
        memcpy(&d_mapping_table[d_mapping_table_size].entry.gpd_id, &db_entry.zgpd_id, sizeof(zb_zgpd_addr_t));
        auto app_id = (db_entry.endpoint == 0) ? ZB_ZGP_APP_ID_0000 : ZB_ZGP_APP_ID_0010;
        d_mapping_table[d_mapping_table_size].entry.options = ZB_ZGP_MAPPING_ENTRY_OPTIONS(app_id, 0);
        d_mapping_table[d_mapping_table_size].entry.gpd_endpoint = db_entry.endpoint;
        d_mapping_table[d_mapping_table_size].entry.gpd_command = ZB_GPDF_CMD_UNDEFINED;
        d_mapping_table[d_mapping_table_size].entry.endpoint = ZB_ZGP_MAPPING_ENTRY_ENDPOINT_PASS_RAW_GPDF_TO_APP;
        d_mapping_table[d_mapping_table_size].entry.profile = ZB_AF_GP_PROFILE_ID;
        d_mapping_table[d_mapping_table_size].entry.cluster = ZB_ZCL_CLUSTER_ID_GREEN_POWER;
        d_mapping_table[d_mapping_table_size].entry.zcl_command = ZB_ZGP_ZCL_CMD_ID_UNDEFINED;
        d_mapping_table[d_mapping_table_size].entry.zcl_payload_length = ZB_ZGP_MAPPING_ENTRY_GPDF_PAYLOAD;
        d_mapping_table_ptrs[d_mapping_table_size] = &d_mapping_table[d_mapping_table_size].entry;
        d_mapping_table_size++;
    }
    ZB_ZGP_SET_MAPPING_TABLE(d_mapping_table_ptrs, &d_mapping_table_size);
    ESP_LOGI(TAG, "GreenPower Mapping Table size: %d of %d", d_mapping_table_size, ZB_ZGP_SINK_TBL_SIZE);
    return ESP_OK;
}

int zb_ncp::get_gp_device_mapping_table_idx(zb_zgpd_id_t* zgpd_id) {
    if (unlikely(d_mapping_table_size == 0 || !d_mapping_table)) {
        return -1;
    }
    const auto app_id = zgpd_id->app_id;
    const uint16_t size = d_mapping_table_size;
    if (app_id == ZB_ZGP_APP_ID_0000) {
        const auto target_src_id = zgpd_id->addr.src_id;
        for (uint16_t i = 0; i < size; ++i) {
            if (d_mapping_table[i].entry.gpd_id.src_id == target_src_id) return i;
        }

    } else if (app_id == ZB_ZGP_APP_ID_0010) {
        uint32_t t0, t1;
        memcpy(&t0, zgpd_id->addr.ieee_addr, sizeof(t0));
        memcpy(&t1, &zgpd_id->addr.ieee_addr[sizeof(t0)], sizeof(t1));
        const auto target_endpoint = zgpd_id->endpoint;
        for (uint16_t i = 0; i < size; ++i) {
            uint32_t c0;
            memcpy(&c0, d_mapping_table[i].entry.gpd_id.ieee_addr, sizeof(c0));
            if (c0 != t0) continue;
            memcpy(&c0, &d_mapping_table[i].entry.gpd_id.ieee_addr[sizeof(c0)], sizeof(c0));
            if (c0 == t1 && d_mapping_table[i].entry.gpd_endpoint == target_endpoint) return i;
        }
    } else if (app_id == ZB_ZGP_APP_ID_0001) {
        uint32_t t0, t1;
        memcpy(&t0, zgpd_id->addr.ieee_addr, sizeof(t0));
        memcpy(&t1, &zgpd_id->addr.ieee_addr[sizeof(t0)], sizeof(t1));
        for (uint16_t i = 0; i < size; ++i) {
            uint32_t c0;
            memcpy(&c0, d_mapping_table[i].entry.gpd_id.ieee_addr, sizeof(c0));
            if (c0 != t0) continue;
            memcpy(&c0, &d_mapping_table[i].entry.gpd_id.ieee_addr[sizeof(c0)], sizeof(c0));
            if (c0 == t1) return i;
        }
    } 
    return -1;
}

bool zb_ncp::add_gp_device_to_mapping_table(zb_zgpd_id_t* zgpd_id) {
    if (get_gp_device_mapping_table_idx(zgpd_id) >= 0) {
        ESP_LOGW(TAG, "Device with src_id: %08lx already exists in Mapping Table", zgpd_id->addr.src_id);
        return false;
    }
    zb_uint16_t count = d_mapping_table_size + 1;
    if (count > ZB_ZGP_SINK_TBL_SIZE) {
        ESP_LOGE(TAG, "Add GreenPower device: Mapping Table is full");
        return false;
    }
    auto _table = static_cast<gp_mapping_table_t*>(realloc(d_mapping_table, count * sizeof(gp_mapping_table_t)));
    if (!_table) {
        ESP_LOGE(TAG, "Add GreenPower device: out of memory (structures)");
        return false;
    }
    d_mapping_table = _table;
    auto _ptrs = static_cast<const zb_zgps_mapping_entry_t**>(realloc(d_mapping_table_ptrs, count * sizeof(zb_zgps_mapping_entry_t*)));
    if (!_ptrs) {
        d_mapping_table = static_cast<gp_mapping_table_t*>(realloc(d_mapping_table, d_mapping_table_size * sizeof(gp_mapping_table_t)));
        ESP_LOGE(TAG, "Add GreenPower device: out of memory (pointers)");
        return false;
    }
    d_mapping_table_ptrs = _ptrs;
    memset(&d_mapping_table[d_mapping_table_size], 0, sizeof(gp_mapping_table_t));
    memcpy(&d_mapping_table[d_mapping_table_size].entry.gpd_id, &zgpd_id->addr, sizeof(zb_zgpd_addr_t));
    d_mapping_table[d_mapping_table_size].entry.options = ZB_ZGP_MAPPING_ENTRY_OPTIONS(zgpd_id->app_id, 0);
    d_mapping_table[d_mapping_table_size].entry.gpd_endpoint = zgpd_id->endpoint;
    d_mapping_table[d_mapping_table_size].entry.gpd_command = ZB_GPDF_CMD_UNDEFINED;
    d_mapping_table[d_mapping_table_size].entry.endpoint = ZB_ZGP_MAPPING_ENTRY_ENDPOINT_PASS_RAW_GPDF_TO_APP;
    d_mapping_table[d_mapping_table_size].entry.profile = ZB_AF_GP_PROFILE_ID;
    d_mapping_table[d_mapping_table_size].entry.cluster = ZB_ZCL_CLUSTER_ID_GREEN_POWER;
    d_mapping_table[d_mapping_table_size].entry.zcl_command = ZB_ZGP_ZCL_CMD_ID_UNDEFINED;
    d_mapping_table[d_mapping_table_size].entry.zcl_payload_length = ZB_ZGP_MAPPING_ENTRY_GPDF_PAYLOAD;
    d_mapping_table_size++;
    for (zb_uint16_t i = 0; i < d_mapping_table_size; ++i) {
        d_mapping_table_ptrs[i] = &d_mapping_table[i].entry;
    }
    ZB_ZGP_SET_MAPPING_TABLE(d_mapping_table_ptrs, &d_mapping_table_size);
    ESP_LOGI(TAG, "Device with src_id: %08lx added to Mapping Table as entry: %d of %d", zgpd_id->addr.src_id, d_mapping_table_size, ZB_ZGP_SINK_TBL_SIZE);
    return true;
}

bool zb_ncp::remove_gp_device_from_mapping_table(zb_zgpd_id_t* zgpd_id) {
    int target_idx = get_gp_device_mapping_table_idx(zgpd_id);
    if (target_idx < 0) {
        ESP_LOGW(TAG, "Device with src_id: %08lx not found in Mapping Table", zgpd_id->addr.src_id);
        return true;
    }
    zb_uint16_t num_elements_to_move = d_mapping_table_size - target_idx - 1;
    if (num_elements_to_move > 0) {
        memmove(&d_mapping_table[target_idx], &d_mapping_table[target_idx + 1], num_elements_to_move * sizeof(gp_mapping_table_t));
    }
    d_mapping_table_size--;
    if (d_mapping_table_size == 0) {
        if (d_mapping_table) {
            free(d_mapping_table);
            d_mapping_table = nullptr;
        }
        d_mapping_table_ptrs = static_cast<const zb_zgps_mapping_entry_t**>(realloc(d_mapping_table_ptrs, sizeof(zb_zgps_mapping_entry_t*)));
        if (d_mapping_table_ptrs) {
            d_mapping_table_ptrs[0] = nullptr;
        }
    } else {
        d_mapping_table = static_cast<gp_mapping_table_t*>(realloc(d_mapping_table, d_mapping_table_size * sizeof(gp_mapping_table_t)));
        d_mapping_table_ptrs = static_cast<const zb_zgps_mapping_entry_t**>(realloc(d_mapping_table_ptrs, d_mapping_table_size * sizeof(zb_zgps_mapping_entry_t*)));
        for (zb_uint16_t i = 0; i < d_mapping_table_size; ++i) {
            d_mapping_table_ptrs[i] = &d_mapping_table[i].entry;
        }
    }
    ZB_ZGP_SET_MAPPING_TABLE(d_mapping_table_ptrs, &d_mapping_table_size);
    ESP_LOGI(TAG, "Device with src_id: %08lx removed from Mapping Table. Size: %d of %d", zgpd_id->addr.src_id, d_mapping_table_size, ZB_ZGP_SINK_TBL_SIZE);
    return true;
}

void zb_ncp::delete_gp_device(uint8_t* payload, uint16_t payload_len) {
    uint32_t options = ((uint32_t)payload[3]) | ((uint32_t)payload[4] << 8) | ((uint32_t)payload[5] << 16);
    if (((options & 0x000008) != 0) || ((options & 0x000010) == 0)) {
        return;
    }
    uint8_t app_id = options & 0x000007;
    zb_zgpd_id_t zgpd_id {
        .app_id = app_id,
        .endpoint = 0,
        .addr = {}
    };
    uint8_t ptr = 6;
    if (app_id == ZB_ZGP_APP_ID_0000) {
        zgpd_id.addr.src_id = *reinterpret_cast<zb_uint32_t*>(&payload[ptr]);
        ptr += sizeof(zb_uint32_t);
    } else {
        memcpy(zgpd_id.addr.ieee_addr, &payload[ptr], sizeof(zb_ieee_addr_t));
        ptr += sizeof(zb_ieee_addr_t);
        if (app_id == ZB_ZGP_APP_ID_0010) {
            zgpd_id.endpoint = payload[ptr];
            ptr += sizeof(zb_uint8_t);
        }
    }
    if (payload_len < ptr) {
        ESP_LOGE(TAG, "delete_gp_device(): Payload length %d is less than parsed structure size %d", payload_len, ptr);
        return;
    }
    zb_uint8_t buf_ref = zb_buf_get_out();
    if (buf_ref == ZB_BUF_INVALID) {
        ESP_LOGE(TAG, "Failed to allocate buffer for GreenPower deletion");
        return;
    }
    zb_zgps_delete_zgpd(buf_ref, &zgpd_id);
    if (!remove_gp_device_from_mapping_table(&zgpd_id)) {
        ESP_LOGE(TAG, "Failed to remove GreenPower device with src_id: %08lx", zgpd_id.addr.src_id);
    }
}

bool zb_ncp::gp_device_commissioning(zb_zgpd_id_t* zgpd_id, zb_uint8_t device_id, zb_uint16_t manuf_id, zb_uint16_t manuf_model_id, zb_ieee_addr_t ieee_addr) {
    int target_idx = get_gp_device_mapping_table_idx(zgpd_id);
    if (target_idx < 0) {
        ESP_LOGE(TAG, "COMM: GreenPower Device with src_id: %08lx not found in Mapping Table", zgpd_id->addr.src_id);
        return false;
    }
    auto t_entry = &d_mapping_table[target_idx];
    auto c_data = ZGP_CTXC().comm_data;
    uint8_t buf_len = zb_buf_len(c_data.comm_req_buf);
    auto buf_ptr = static_cast<uint8_t*>(zb_buf_begin(c_data.comm_req_buf));
    auto ind = ZB_BUF_GET_PARAM(c_data.comm_req_buf, zb_gpdf_info_t);
    if (!ind || !buf_ptr || buf_len == 0) {
        ESP_LOGE(TAG, "COMM: GreenPower Device Commissioning with zero buffer");
        return false;
    }
    uint8_t is_manuf_specific = (device_id == 0xFE || device_id == 0xFF) ? 1 : 0;
    uint8_t cmd_direction = ZB_GPDF_EXT_NFC_GET_DIRECTION(ind->nwk_ext_frame_ctl);
    uint8_t zcl_hdr[5];
    uint8_t zcl_hdr_len = 0;
    uint8_t zcl_hdr_fc = ZB_ZCL_CONSTRUCT_FRAME_CONTROL(
        ZB_ZCL_FRAME_TYPE_CLUSTER_SPECIFIC,
        is_manuf_specific,
        cmd_direction,
        ZB_ZCL_DISABLE_DEFAULT_RESPONSE
    );
    zcl_hdr[zcl_hdr_len++] = zcl_hdr_fc;
    if (is_manuf_specific == 1) {
        memcpy(&zcl_hdr[zcl_hdr_len], &manuf_id, sizeof(manuf_id));
        zcl_hdr_len += sizeof(manuf_id);
    }
    zcl_hdr[zcl_hdr_len++] = ind->mac_seq_num;
    zcl_hdr[zcl_hdr_len++] = ZGP_SERVER_CMD_GP_COMMISSIONING_NOTIFICATION;
    zb_uint16_t dst_nwk;
    if (ind->mac_addr_flds_len == 2 || ind->mac_addr_flds_len == 4) {
        dst_nwk = ind->mac_addr_flds.s.dst_addr;
    } else if (ind->mac_addr_flds_len == 8) {
        dst_nwk = zb_address_short_by_ieee(ind->mac_addr_flds.l.addr);
    } else {
        dst_nwk = ind->mac_addr_flds.comb.dst_addr;
    }
    zb_uint16_t grp_nwk = 0x0000;
    uint8_t outdata[sizeof(zb_ncp::aps_data_ind_header_t) + zcl_hdr_len + GreenPowerParser::MAX_SIZE + buf_len];
    GreenPowerParser gp_buf(ind->zgpd_id.app_id, &outdata[sizeof(zb_ncp::aps_data_ind_header_t) + zcl_hdr_len]);
    auto hdr = reinterpret_cast<zb_ncp::aps_data_ind_header_t*>(outdata);
    hdr->paramLength = sizeof(zb_ncp::aps_data_ind_header_t) - sizeof(hdr->paramLength) - sizeof(hdr->dataLength);
    hdr->dataLength = zcl_hdr_len + gp_buf.size() + buf_len;
    hdr->apsFC = ind->nwk_frame_ctl;
    hdr->srcNwk = c_data.temp_master_list[c_data.selected_temp_master_idx].short_addr;
    hdr->dstNwk = dst_nwk;
    hdr->grpNwk = grp_nwk;
    hdr->dstEndpoint = (ind->zgpd_id.endpoint == 0) ? ZGP_ENDPOINT : ind->zgpd_id.endpoint;
    hdr->srcEndpoint = ZGP_ENDPOINT;
    hdr->clusterID = t_entry->entry.cluster;
    hdr->profileID = t_entry->entry.profile;
    hdr->apsCounter = ind->mac_seq_num;
    hdr->srcMAC = c_data.temp_master_list[c_data.selected_temp_master_idx].short_addr;
    hdr->dstMAC = c_data.sink_addr;
    hdr->lqi = ind->lqi;
    hdr->rssi = (uint8_t)ind->rssi;
    const uint8_t zb_sec = ZGP_GPS_GET_SECURITY_LEVEL(); // zb_zgps_get_security_level();
    hdr->apsKey = (zb_sec & 0x03) | ((zb_sec >> 2U) << 1) | ((zb_sec >> 3U) << 3) | (0U << 4);
    memcpy(&outdata[sizeof(zb_ncp::aps_data_ind_header_t)], zcl_hdr, zcl_hdr_len);
    uint8_t bidir_cap = (c_data.channel_conf_sent || c_data.comm_reply_sent) ? 1 : 0;
    uint16_t notif_opt = ZB_ZGP_FILL_COMM_NOTIFICATION_OPTIONS(
        ind->zgpd_id.app_id,
        ZB_GPDF_EXT_NFC_GET_RX_AFTER_TX(ind->nwk_ext_frame_ctl),
        ZB_GPDF_EXT_NFC_GET_SEC_LEVEL(ind->nwk_ext_frame_ctl),
        ind->key_type,
        (ind->status != 1 ? 1 : 0),
        bidir_cap,
        0
    );
    gp_buf.set_options(notif_opt);
    gp_buf.set_src_id(ind->zgpd_id.addr.src_id);
    gp_buf.set_endpoint(ind->zgpd_id.endpoint);
    gp_buf.set_ieee_addr(ind->zgpd_id.addr.ieee_addr);
    gp_buf.set_sec_frame_counter(c_data.comm_dup_counter);
    gp_buf.set_zgpd_cmd_id(ind->zgpd_cmd_id);
    gp_buf.set_payload_len(buf_len);
    memcpy(gp_buf.get_end_ptr(), buf_ptr, buf_len);
    memcpy(t_entry->payload, outdata, sizeof(zb_ncp::aps_data_ind_header_t) + hdr->dataLength);
    ESP_LOGD(TAG, "GreenPower SINK Indication: profileId: %04x, clusterId: %04x, srcAddr: %04x, dstAddr: %04x, groupAddr: %04x, srcEndpoint: %u, dstEndpoint: %u, mac_src_addr: %04x, mac_dst_addr: %04x, fc: 0x%02x, options: 0x%04x, lqi: %u, rssi: %d, TSN: %u, dataLen: %u",
        hdr->profileID, hdr->clusterID, hdr->srcNwk, hdr->dstNwk, hdr->grpNwk, hdr->srcEndpoint, hdr->dstEndpoint, hdr->srcMAC, hdr->dstMAC, hdr->apsFC, notif_opt, hdr->lqi, ind->rssi, ind->mac_seq_num, hdr->dataLength
    );
    return true;
}

bool zb_ncp::gp_device_indication(zb_zgpd_id_t* zgpd_id) {
    int target_idx = get_gp_device_mapping_table_idx(zgpd_id);
    if (target_idx < 0) {
        ESP_LOGE(TAG, "COMM: GreenPower Device with src_id: %08lx not found in Mapping Table", zgpd_id->addr.src_id);
        return false;
    }
    auto t_entry = &d_mapping_table[target_idx];
    auto hdr = reinterpret_cast<zb_ncp::aps_data_ind_header_t*>(t_entry->payload);
    if (hdr->dataLength == 0) {
        ESP_LOGD(TAG, "COMM: GreenPower Device with src_id: %08lx has already been paired", zgpd_id->addr.src_id);
        return true;
    }
    uint8_t len = sizeof(zb_ncp::aps_data_ind_header_t) + hdr->dataLength;
    zb_ncp::indication(APSDE_DATA_IND, t_entry->payload, len);
    memset(t_entry->payload, 0, sizeof(t_entry->payload));
    t_entry->sec_frame_counter = 0;
    return true;
}

bool zb_ncp::set_esp_clock(zb_uint32_t zb_time, zb_int32_t zb_time_zone, zb_uint32_t zb_dst_shift, zb_uint32_t zb_dst_start, zb_uint32_t zb_dst_end) {
    if (zb_time == ZB_ZCL_TIME_TIME_INVALID_VALUE) {
        ESP_LOGE(TAG, "Ignoring Invalid Time value");
        return false;
    }
    timeval tv = {
        .tv_sec = static_cast<time_t>(zb_time + ZB_TIME_SHIFT),
        .tv_usec = 0
    };
    if (settimeofday(&tv, nullptr) != 0) {
        ESP_LOGE(TAG, "Failed to set ESP32 time via settimeofday");
        return false;
    }
    int std_offset_hours = static_cast<int>(-(zb_time_zone / 3600));
    int dst_offset_hours = std_offset_hours - static_cast<int>(zb_dst_shift / 3600);
    time_t unix_dst_start = static_cast<time_t>(zb_dst_start + ZB_TIME_SHIFT);
    time_t unix_dst_end = static_cast<time_t>(zb_dst_end + ZB_TIME_SHIFT);
    tm tm_buf{};
    gmtime_r(&unix_dst_start, &tm_buf);
    int day_start = tm_buf.tm_yday;
    int hour_start = tm_buf.tm_hour;
    gmtime_r(&unix_dst_end, &tm_buf);
    int day_end = tm_buf.tm_yday;
    int hour_end = tm_buf.tm_hour;
    char tz_str[64]{};
    snprintf(tz_str, sizeof(tz_str), "STD%+dDST%+d,%d/%d,%d/%d", std_offset_hours, dst_offset_hours, day_start, hour_start, day_end, hour_end);
    setenv("TZ", tz_str, 1);
    tzset();
    return true;
}

void zb_ncp::on_rx_data(const void* data, uint16_t size) {
    auto cmd = static_cast<const cmd_t*>(data);
    if (cmd->type != REQUEST && cmd->type != RESPONSE) {
        ESP_LOGE(TAG, "Indication received from Host");
        return;
    }
    ESP_LOGD(TAG, "Command ID: 0x%04x", cmd->command_id);
    uint16_t len = size - sizeof(cmd_t);
    switch (cmd->command_id) {
#define COMMAND(Name, Val) \
    case Val: \
        cmd_handle<Name>::process(*cmd, cmd + 1, len); \
        break;
        COMMAND_LIST(COMMAND)
#undef COMMAND
    default:
        ESP_LOGE(TAG, "Unknown Command_ID = %04x", cmd->command_id);
        uint8_t outdata[sizeof(cmd_t) + sizeof(generic_response_t)];
        auto out_cmd = reinterpret_cast<cmd_t*>(outdata);
        *out_cmd = *cmd;
        out_cmd->type = RESPONSE;
        auto out_resp = reinterpret_cast<generic_response_t*>(out_cmd + 1);
        out_resp->category = STATUS_CATEGORY_GENERIC;
        out_resp->status = GENERIC_NOT_IMPLEMENTED;
        zb_ncp::send_cmd_data(outdata, sizeof(outdata));
        break;
    }
}

void zb_ncp::send_cmd_data(const void* data, uint16_t size) {
    esp_err_t ret = protocol::send_data(data, size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send CMD data, error: %s", esp_err_to_name(ret));
    }
}

void zb_ncp::indication(command_id_t cmd, const void* data, uint16_t size) {
    uint8_t buffer[sizeof(indication_hdr_t) + size];
    auto hdr = reinterpret_cast<indication_hdr_t*>(buffer);
    hdr->version = DEVICE_VERSION;
    hdr->type = INDICATION;
    hdr->command_id = cmd;
    memcpy(&buffer[sizeof(indication_hdr_t)], data, size);
    esp_err_t ret = protocol::send_data(buffer, sizeof(buffer));
    if (unlikely(ret != ESP_OK)) {
        ESP_LOGE(TAG, "Failed to send Indication, error: %s", esp_err_to_name(ret));
    }
}

void wakeup_callback(zb_uint8_t param) {
    ESP_LOGD(TAG, "Wakeup");
}

void device_attribute_callback(zb_uint8_t param) {
    auto dev_prm = ZB_BUF_GET_PARAM(param, zb_zcl_device_callback_param_t);
    if (!dev_prm) {
        return;
    }
    switch (dev_prm->device_cb_id) {
    case ZB_ZCL_SET_ATTR_VALUE_CB_ID:
        switch (dev_prm->cb_param.set_attr_value_param.cluster_id) {
        case ZB_ZCL_CLUSTER_ID_TIME:
            if (dev_prm->cb_param.set_attr_value_param.attr_id == ZB_ZCL_ATTR_TIME_VALID_UNTIL_TIME_ID) {
                if (zb_ncp::set_esp_clock(dev_ctx.g_attr_time_time, dev_ctx.g_attr_time_time_zone, dev_ctx.g_attr_time_dst_shift, dev_ctx.g_attr_time_dst_start, dev_ctx.g_attr_time_dst_end)) {
                    dev_ctx.g_attr_time_time_status = (1 << ZB_ZCL_TIME_MASTER) | (1 << ZB_ZCL_TIME_MASTER_ZONE_DST) | (1 << ZB_ZCL_TIME_SUPERSEDING);
                    zb_uint32_t current_zb_time = static_cast<zb_uint32_t>(time(nullptr) - ZB_TIME_SHIFT);
                    zb_uint32_t interval = dev_ctx.g_attr_time_valid_until_time - current_zb_time;
                    interval = (interval > 300) ? (interval - 300) : 0;
                    ZB_SCHEDULE_APP_ALARM(clock_update_callback, 0, interval * ZB_TIME_ONE_SECOND);
                }
            }
            else {
                ESP_LOGW(TAG, "TIME CLUSTER attribute: %04x, value: %lu",
                    dev_prm->cb_param.set_attr_value_param.attr_id, dev_prm->cb_param.set_attr_value_param.values.data32
                );
            }
            break;
        default:
            ESP_LOGW(TAG, "ZB_ZCL_SET_ATTR_VALUE_CB_ID: cluster: %04x, attribute: %04x",
                dev_prm->cb_param.set_attr_value_param.cluster_id, dev_prm->cb_param.set_attr_value_param.attr_id
            );
            break;
        }
        break;
    case ZB_ZCL_OTA_UPGRADE_VALUE_CB_ID:
        {
            auto resp = &dev_prm->cb_param.ota_value_param;
            if (resp->upgrade_status == ZB_ZCL_OTA_UPGRADE_STATUS_START) {
                ESP_LOGD(TAG, "START upgrade. Manufacturer: 0x%04x, Firmware Ver: 0x%08X, File size: %lu bytes", resp->upgrade.start.manufacturer, resp->upgrade.start.file_version, resp->upgrade.start.file_length);
                zb_ncp::reset_ota_context();
                if (resp->upgrade.start.manufacturer == OTA_MANUFACTURER && resp->upgrade.start.image_type == OTA_IMAGE_TYPE && resp->upgrade.start.file_version > OTA_APP_VERSION && resp->upgrade.start.file_length < OTA_PARTITION_SIZE) {
                    zb_ncp::m_update_partition = esp_ota_get_next_update_partition(NULL);
                    if (!zb_ncp::m_update_partition) {
                        ESP_LOGE(TAG, "Inactive OTA Partition not found in partition table.");
                        resp->upgrade_status = ZB_ZCL_OTA_UPGRADE_STATUS_ERROR;
                        break;
                    }
                    esp_err_t err = esp_ota_begin(zb_ncp::m_update_partition, OTA_SIZE_UNKNOWN, &zb_ncp::m_ota_handle);
                    if (err != ESP_OK) {
                        ESP_LOGE(TAG, "Failed to open OTA Partition session: %s", esp_err_to_name(err));
                        zb_ncp::reset_ota_context();
                        resp->upgrade_status = ZB_ZCL_OTA_UPGRADE_STATUS_ERROR;
                    } else {
                        ESP_LOGI(TAG, "OTA Partition session opened. Partition %s is ready to be filled.", zb_ncp::m_update_partition->label);
                        resp->upgrade_status = ZB_ZCL_OTA_UPGRADE_STATUS_OK;
                    }
                } else {
                    resp->upgrade_status = ZB_ZCL_OTA_UPGRADE_STATUS_ERROR;
                }
            } else if (resp->upgrade_status == ZB_ZCL_OTA_UPGRADE_STATUS_RECEIVE) {
                const uint32_t offset = resp->upgrade.receive.file_offset;
                uint8_t len = resp->upgrade.receive.data_length;
                uint8_t* data_ptr = resp->upgrade.receive.block_data;
                uint32_t write_offset = 0;
                if (offset < OTA_IMAGE_HEADER_LEN) {
                    if ((offset + len) > OTA_IMAGE_HEADER_LEN) {
                        const uint32_t header_bytes_left = OTA_IMAGE_HEADER_LEN - offset;
                        data_ptr += header_bytes_left;
                        len -= header_bytes_left;
                        write_offset = 0;
                    } else {
                        len = 0;
                    }
                } else {
                    write_offset = offset - OTA_IMAGE_HEADER_LEN;
                }
                if (len > 0 && zb_ncp::m_ota_handle != 0) {
                    esp_err_t err = esp_ota_write_with_offset(zb_ncp::m_ota_handle, data_ptr, len, write_offset);
                    if (err != ESP_OK) {
                        ESP_LOGE(TAG, "OTA Partition write error on offset %lu: %s", write_offset, esp_err_to_name(err));
                        zb_ncp::reset_ota_context();
                        resp->upgrade_status = ZB_ZCL_OTA_UPGRADE_STATUS_ERROR;
                        break;
                    }
                }
                resp->upgrade_status = ZB_ZCL_OTA_UPGRADE_STATUS_OK;
            } else if (resp->upgrade_status == ZB_ZCL_OTA_UPGRADE_STATUS_ABORT) {
                zb_ncp::reset_ota_context();
                resp->upgrade_status = ZB_ZCL_OTA_UPGRADE_STATUS_OK;
            } else if (resp->upgrade_status == ZB_ZCL_OTA_UPGRADE_STATUS_CHECK) {
                if (zb_ncp::m_ota_handle != 0) {
                    esp_err_t err = esp_ota_end(zb_ncp::m_ota_handle);
                    if (err != ESP_OK) {
                        if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
                            ESP_LOGE(TAG, "The bootloader rejected the SHA256 signature of the firmware on OTA Partition.");
                        } else {
                            ESP_LOGE(TAG, "Failed to complete OTA Partition: %s", esp_err_to_name(err));
                        }
                        zb_ncp::reset_ota_context();
                        resp->upgrade_status = ZB_ZCL_OTA_UPGRADE_STATUS_ERROR;
                        break;
                    }
                    zb_ncp::m_ota_handle = 0;
                    resp->upgrade_status = ZB_ZCL_OTA_UPGRADE_STATUS_OK;
                } else {
                    resp->upgrade_status = ZB_ZCL_OTA_UPGRADE_STATUS_ERROR;
                }
            } else if (resp->upgrade_status == ZB_ZCL_OTA_UPGRADE_STATUS_APPLY) {
                if (zb_ncp::m_update_partition) {
                    esp_err_t err = esp_ota_set_boot_partition(zb_ncp::m_update_partition);
                    if (err != ESP_OK) {
                        ESP_LOGE(TAG, "Failed to set Boot OTA Partition: %s", esp_err_to_name(err));
                        zb_ncp::reset_ota_context();
                        resp->upgrade_status = ZB_ZCL_OTA_UPGRADE_STATUS_ERROR;
                        break;
                    }
                    ESP_LOGI(TAG, "Boot OTA Partition has been successfully configured to '%s'.", zb_ncp::m_update_partition->label);
                }
                resp->upgrade_status = ZB_ZCL_OTA_UPGRADE_STATUS_OK;
            } else if (resp->upgrade_status == ZB_ZCL_OTA_UPGRADE_STATUS_FINISH) {
                zb_ncp::reset_ota_context();
                resp->upgrade_status = ZB_ZCL_OTA_UPGRADE_STATUS_OK;
                zdo_device_annce_params_t params = {
                    .dev_short_addr = 0x0000,
                    .dev_ieee = {},
                    .capability = 0x8F
                };
                zb_ieee_addr_t ieee_addr;
                zb_get_long_address(ieee_addr);
                memcpy(params.dev_ieee, ieee_addr, sizeof(zb_ieee_addr_t));
                zb_ncp::indication(ZDO_DEV_ANNCE_IND, &params, sizeof(zdo_device_annce_params_t));
                app::ctx_t ncp_event = {
                    .event = app::EVENT_RESET,
                    .size = 2000, // Pause 2000 ms before Restart
                    .buf_ptr = nullptr
                };
                app::send_event(ncp_event);
            }
        }
        break;
    case ZB_ZCL_OTA_UPGRADE_QUERY_IMAGE_RESP_CB_ID:
        {
            auto resp = &dev_prm->cb_param.ota_upgrade_query_img_resp_param;
            if (resp->status == ZB_ZCL_STATUS_NO_IMAGE_AVAILABLE) {
                ESP_LOGI(TAG, "There are no firmware updates for the Coordinator at this moment.");
                break;
            }
            ESP_LOGD(TAG, "OTA QUERY_IMAGE_RESP Server endpoint: %u, Manufacturer: 0x%04x, Firmware Ver: 0x%08X, File size: %lu bytes, Status: %d", resp->server_endpoint, resp->manufacturer, resp->file_version, resp->image_size, resp->status);
            if (!(resp->server_endpoint == COORDINATOR_ENDPOINT && resp->manufacturer == OTA_MANUFACTURER && resp->image_type == OTA_IMAGE_TYPE && resp->file_version > OTA_APP_VERSION && resp->image_size < OTA_PARTITION_SIZE)) {
                resp->status = ZB_ZCL_STATUS_NO_IMAGE_AVAILABLE;
            }
        }
        break;
    default:
        ESP_LOGW(TAG, "device_attribute_callback(): cluster: %04x, attribute: %04x, callback_id: %u",
            dev_prm->cb_param.set_attr_value_param.cluster_id, dev_prm->cb_param.set_attr_value_param.attr_id, (unsigned int)dev_prm->device_cb_id
        );
        break;
    }
}

void firmware_upgrade_callback(zb_uint8_t param) {
    auto buf = zb_buf_get_out();
    if (!buf) {
        ESP_LOGE(TAG, "Cannot allocate out buffer for OTA Query");
        return;
    }
    zb_uint16_t target_short_addr = 0x0000;
    zb_uint8_t frame_control = ZB_ZCL_CONSTRUCT_FRAME_CONTROL(
        ZB_ZCL_FRAME_TYPE_COMMON, // ZB_ZCL_FRAME_TYPE_CLUSTER_SPECIFIC
        ZB_ZCL_NOT_MANUFACTURER_SPECIFIC,
        ZB_ZCL_FRAME_DIRECTION_TO_SRV,
        ZB_ZCL_DISABLE_DEFAULT_RESPONSE
    );
    zb_uint8_t* ptr = static_cast<zb_uint8_t*>(zb_zcl_start_command_header(
        buf,
        frame_control,
        0,
        ZB_ZCL_CMD_REPORT_ATTRIB, // ZB_ZCL_CMD_OTA_UPGRADE_QUERY_NEXT_IMAGE_ID
        nullptr
    ));
    ptr = static_cast<zb_uint8_t*>(zb_put_next_htole16(ptr, ZB_ZCL_ATTR_OTA_UPGRADE_FILE_VERSION_ID));
    *ptr = ZB_ZCL_ATTR_TYPE_U32; ptr++;
    ptr = static_cast<zb_uint8_t*>(zb_put_next_htole32(ptr, OTA_APP_VERSION));
    ptr = static_cast<zb_uint8_t*>(zb_put_next_htole16(ptr, ZB_ZCL_ATTR_OTA_UPGRADE_MANUFACTURE_ID));
    *ptr = ZB_ZCL_ATTR_TYPE_U16; ptr++;
    ptr = static_cast<zb_uint8_t*>(zb_put_next_htole16(ptr, OTA_MANUFACTURER));
    ptr = static_cast<zb_uint8_t*>(zb_put_next_htole16(ptr, ZB_ZCL_ATTR_OTA_UPGRADE_IMAGE_TYPE_ID));
    *ptr = ZB_ZCL_ATTR_TYPE_U16; ptr++;
    ptr = static_cast<zb_uint8_t*>(zb_put_next_htole16(ptr, OTA_IMAGE_TYPE));
    ptr = static_cast<zb_uint8_t*>(zb_put_next_htole16(ptr, ZB_ZCL_ATTR_OTA_UPGRADE_MIN_BLOCK_REQUE_ID));
    *ptr = ZB_ZCL_ATTR_TYPE_U16; ptr++;
    ptr = static_cast<zb_uint8_t*>(zb_put_next_htole16(ptr, (OTA_MAX_DATA_SIZE - 18)));
    ptr = static_cast<zb_uint8_t*>(zb_put_next_htole16(ptr, ZB_ZCL_ATTR_OTA_UPGRADE_STACK_VERSION_ID));
    *ptr = ZB_ZCL_ATTR_TYPE_U16; ptr++;
    ptr = static_cast<zb_uint8_t*>(zb_put_next_htole16(ptr, OTA_HARDWARE_VERSION));
    zb_zcl_finish_and_send_packet_new(
        buf,
        ptr,
        reinterpret_cast<const zb_addr_u*>(&target_short_addr),
        ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
        COORDINATOR_ENDPOINT,
        COORDINATOR_ENDPOINT,
        ZB_AF_HA_PROFILE_ID,
        ZB_ZCL_CLUSTER_ID_OTA_UPGRADE,
        nullptr,
        ZB_FALSE,
        ZB_FALSE,
        0
    );
}

void clock_update_callback(zb_uint8_t param) {
    auto buf = zb_buf_get_out();
    if (!buf) {
        ESP_LOGE(TAG, "Cannot allocate out buffer for Time Sync Request");
        return;
    }
    zb_uint16_t target_short_addr = 0x0000;
    zb_uint8_t frame_control = ZB_ZCL_CONSTRUCT_FRAME_CONTROL(
        ZB_ZCL_FRAME_TYPE_COMMON,
        ZB_ZCL_NOT_MANUFACTURER_SPECIFIC,
        ZB_ZCL_FRAME_DIRECTION_TO_CLI,
        ZB_ZCL_DISABLE_DEFAULT_RESPONSE
    );
    zb_uint8_t* ptr = static_cast<zb_uint8_t*>(zb_zcl_start_command_header(
        buf,
        frame_control,
        0,
        ZB_ZCL_CMD_REPORT_ATTRIB,
        nullptr
    ));
    ptr = static_cast<zb_uint8_t*>(zb_put_next_htole16(ptr, ZB_ZCL_ATTR_TIME_TIME_ID));
    *ptr = ZB_ZCL_ATTR_TYPE_U32; ptr++;
    ptr = static_cast<zb_uint8_t*>(zb_put_next_htole32(ptr, ZB_ZCL_TIME_TIME_DEFAULT_VALUE));
    dev_ctx.g_attr_time_time_status = ZB_ZCL_TIME_TIME_STATUS_DEFAULT_VALUE;
    zb_zcl_finish_and_send_packet_new(
        buf,
        ptr,
        reinterpret_cast<const zb_addr_u*>(&target_short_addr),
        ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
        COORDINATOR_ENDPOINT,
        COORDINATOR_ENDPOINT,
        ZB_AF_HA_PROFILE_ID,
        ZB_ZCL_CLUSTER_ID_TIME,
        nullptr,
        ZB_FALSE,
        ZB_FALSE,
        0
    );
}

static_assert(sizeof(zb_apsde_data_indication_t) == 32);
zb_uint8_t data_indication_callback(zb_uint8_t param) {
    zb_ret_t ret = zb_buf_get_status(param);
    if (unlikely(ret != RET_OK)) {
        ESP_LOGE(TAG, "Indication failed (error: %d)", ERROR_GET_CODE(ret));
        return ZB_FALSE;
    }
    auto ind = ZB_BUF_GET_PARAM(param, const zb_apsde_data_indication_t);
    if (unlikely(!ind || ind->src_endpoint == 0)) {
        return ZB_FALSE;
    }
    auto len = zb_buf_len(param);
    ESP_LOGD(TAG, "Indication: profileId: %04x, clusterId: %04x, srcAddr: %04x, dstAddr: %04x, groupAddr: %04x, srcEndpoint: %d, dstEndpoint: %d, mac_src_addr: %04x, mac_dst_addr: %04x, fc: %02x, TSN: %d, dataLen: %d",
        ind->profileid, ind->clusterid, ind->src_addr, ind->dst_addr, ind->group_addr, ind->src_endpoint, ind->dst_endpoint, ind->mac_src_addr, ind->mac_dst_addr, ind->fc, ind->aps_counter, len
    );
    uint8_t outdata[sizeof(zb_ncp::aps_data_ind_header_t) + len];
    auto hdr = reinterpret_cast<zb_ncp::aps_data_ind_header_t*>(outdata);
    hdr->paramLength = sizeof(zb_ncp::aps_data_ind_header_t) - sizeof(hdr->paramLength) - sizeof(hdr->dataLength);
    hdr->dataLength = len;
    hdr->apsFC = ind->fc;
    hdr->srcNwk = ind->src_addr;
    hdr->dstNwk = ind->dst_addr;
    hdr->grpNwk = ind->group_addr;
    hdr->dstEndpoint = ind->dst_endpoint;
    hdr->srcEndpoint = ind->src_endpoint;
    hdr->clusterID = ind->clusterid;
    hdr->profileID = ind->profileid;
    hdr->apsCounter = ind->aps_counter;
    hdr->srcMAC = ind->mac_src_addr;
    hdr->dstMAC = ind->mac_dst_addr;
    hdr->lqi = ind->lqi;
    hdr->rssi = (ind->rssi != 0) ? (uint8_t)ind->rssi : (uint8_t)((ind->lqi / 3) - 100);
    hdr->apsKey = ind->aps_key_source | (ind->aps_key_attrs << 1) | (ind->aps_key_from_tc << 3) | (ind->extended_fc << 4);
    memcpy(&outdata[sizeof(zb_ncp::aps_data_ind_header_t)], zb_buf_begin(param), len);
    zb_ncp::indication(APSDE_DATA_IND, outdata, sizeof(outdata));
    return ZB_FALSE;
}

void commissioning_callback(zb_uint8_t param) {
    ESP_LOGD(TAG, "Commissioning Network %s", param == ZB_BDB_NETWORK_FORMATION ? "FORMATION" : "STEERING");
    if (!bdb_start_top_level_commissioning(param)) {
        ESP_LOGE(TAG, "Commissioning failed: GENERIC_ERROR");
        param == ZB_BDB_NETWORK_FORMATION ?
            zb_ncp::cmd_handle<NWK_FORMATION>::response(GENERIC_ERROR) :
            zb_ncp::cmd_handle<NWK_START_WITHOUT_FORMATION>::response(GENERIC_ERROR);
    }
}

void begin_callback(zb_uint8_t param) {
    if (dev_ctx.g_init_done) return;
    if (param == 1) {
        dev_ctx.g_init_module_ver_count++;
    } else {
        dev_ctx.g_init_coord_ver_count++;
    }
    if (dev_ctx.g_init_module_ver_count >= 2 && dev_ctx.g_init_coord_ver_count >= 2) {
        dev_ctx.g_init_done = ZB_TRUE;
        if (ZB_SCHEDULE_APP_ALARM(clock_update_callback, 0, 1 * ZB_TIME_ONE_SECOND) != RET_OK) {
            ESP_LOGE(TAG, "SCHEDULE firmware upgrade failed: out of memory");
        }
        if (ZB_SCHEDULE_APP_ALARM(firmware_upgrade_callback, 0, 15 * ZB_TIME_ONE_SECOND) != RET_OK) {
            ESP_LOGE(TAG, "SCHEDULE firmware upgrade failed: out of memory");
        }
    }
}

void zboss_signal_handler(zb_uint8_t param) {
    if (unlikely(!param)) return;
    zb_zdo_app_signal_hdr_t* sg_p = NULL;
    zb_zdo_app_signal_type_t sig = zb_get_app_signal(param, &sg_p);
    zb_ret_t status = ZB_GET_APP_SIGNAL_STATUS(param);
    bool success = status == RET_OK;
    switch (sig) {
    case ZB_ZDO_SIGNAL_DEFAULT_START:
    case ZB_ZDO_SIGNAL_SKIP_STARTUP:
        {
            ESP_LOGI(TAG, "Initialize Zigbee Stack");
            zb_af_set_data_indication(data_indication_callback);
            zb_uint32_t func, act_func;
            zb_zgp_get_sink_functionality(&func, &act_func);
            ESP_LOGI(TAG, "Sink functionality: %08lx, Active sink functionality: %08lx", func, act_func);
            zb_uint8_t req = ZB_BDB_NETWORK_STEERING;
            if (zb_ncp::cmd_handle<NWK_FORMATION>::need_resolve()) {
                req = ZB_BDB_NETWORK_FORMATION;
            }
            if (ZB_SCHEDULE_APP_CALLBACK(commissioning_callback, req) != RET_OK) {
                ESP_LOGE(TAG, "Commissioning failed: out of memory");
            }
        }
        break;
    case ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ZB_BDB_SIGNAL_DEVICE_REBOOT:
        if (success) {
            ESP_LOGI(TAG, "Network restored from NVRAM.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize Zigbee Stack (error: %d)", ERROR_GET_CODE(status));
        }
        zb_ncp::cmd_handle<NWK_START_WITHOUT_FORMATION>::response(success ? GENERIC_OK : static_cast<ncp_generic_status_t>(ERROR_GET_CODE(status)));
        break;
    case ZB_ZDO_SIGNAL_DEVICE_ANNCE:
        {
            zdo_device_annce_params_t params;
            {
                auto parameters = ZB_ZDO_SIGNAL_GET_PARAMS(sg_p, const zb_zdo_signal_device_annce_params_t);
                ESP_LOGD(TAG, "ZB_ZDO_SIGNAL_DEVICE_ANNCE long_addr: " IEEE_ADDR_FMT ", short_addr: %04x, capability: %d", IEEE_ADDR_PRINT(parameters->ieee_addr), parameters->device_short_addr, parameters->capability);
                params.dev_short_addr = parameters->device_short_addr;
                memcpy(params.dev_ieee, parameters->ieee_addr, sizeof(parameters->ieee_addr));
                params.capability = parameters->capability;
            }
            zb_ncp::indication(ZDO_DEV_ANNCE_IND, &params, sizeof(zdo_device_annce_params_t));
        }
        break;
    case ZB_ZDO_SIGNAL_LEAVE:
        {
            auto parameters = ZB_ZDO_SIGNAL_GET_PARAMS(sg_p, const zb_zdo_signal_leave_params_t);
            ESP_LOGD(TAG, "ZB_ZDO_SIGNAL_LEAVE type: %s", parameters->leave_type == ZB_NWK_LEAVE_TYPE_REJOIN ? "Leave with rejoin" : "Leave without rejoin");
        }
        break;
    case ZB_ZDO_SIGNAL_LEAVE_INDICATION:
        {
            zdo_device_leave_params_t params;
            {
                auto parameters = ZB_ZDO_SIGNAL_GET_PARAMS(sg_p, const zb_zdo_signal_leave_indication_params_t);
                memcpy(params.device_ieee, parameters->device_addr, sizeof(parameters->device_addr));
                params.rejoin = parameters->rejoin;
                ESP_LOGD(TAG, "ZB_ZDO_SIGNAL_LEAVE_INDICATION short_addr: %04x, rejoin: %d", parameters->short_addr, parameters->rejoin);
            }
            zb_ncp::indication(NWK_LEAVE_IND, &params, sizeof(zdo_device_leave_params_t));
        }
        break;
    case ZB_ZDO_SIGNAL_ERROR:
        ESP_LOGE(TAG, "ZB_ZDO_SIGNAL_ERROR (error: %d)", ERROR_GET_CODE(status));
        break;
    case ZB_BDB_SIGNAL_STEERING:
        if (success) {
            ESP_LOGI(TAG, "Successfull steering");
        } else {
            ESP_LOGE(TAG, "ZB_BDB_SIGNAL_STEERING failed (error: %d)", ERROR_GET_CODE(status));
        }
        break;
    case ZB_BDB_SIGNAL_FORMATION:
        if (success) {
            zb_ext_pan_id_t ext_pan_id;
            zb_get_extended_pan_id(ext_pan_id);
            ESP_LOGI(TAG, "Formed network successfully (ExtPanID: " IEEE_ADDR_FMT ", PanID: %04x, RadioChannel: %d)", IEEE_ADDR_PRINT(ext_pan_id), zb_get_pan_id(), zb_get_current_channel());
        } else {
            ESP_LOGE(TAG, "Network formation failed (error: %d)", ERROR_GET_CODE(status));
        }
        zb_ncp::cmd_handle<NWK_FORMATION>::response(success ? GENERIC_OK : static_cast<ncp_generic_status_t>(ERROR_GET_CODE(status)));
        break;
    case ZB_BDB_SIGNAL_FINDING_AND_BINDING_TARGET_FINISHED:
        ESP_LOGD(TAG, "ZB_BDB_SIGNAL_FINDING_AND_BINDING_TARGET_FINISHED");
        break;
    case ZB_BDB_SIGNAL_FINDING_AND_BINDING_INITIATOR_FINISHED:
        ESP_LOGD(TAG, "ZB_BDB_SIGNAL_FINDING_AND_BINDING_INITIATOR_FINISHED");
        break;
    case ZB_NWK_SIGNAL_DEVICE_ASSOCIATED:
        {
            auto parameters = ZB_ZDO_SIGNAL_GET_PARAMS(sg_p, const zb_nwk_signal_device_associated_params_t);
            ESP_LOGD(TAG, "ZB_NWK_SIGNAL_DEVICE_ASSOCIATED addr: " IEEE_ADDR_FMT, IEEE_ADDR_PRINT(parameters->device_addr));
        }
        break;
    case ZB_BDB_SIGNAL_WWAH_REJOIN_STARTED:
        ESP_LOGD(TAG, "ZB_BDB_SIGNAL_WWAH_REJOIN_STARTED");
        break;
    case ZB_ZGP_SIGNAL_COMMISSIONING:
        {
            auto params = ZB_ZDO_SIGNAL_GET_PARAMS(sg_p, zb_zgp_signal_commissioning_params_t);
            const char* result;
            switch (params->result) {
            case ZB_ZGP_COMMISSIONING_COMPLETED:
                if (zb_ncp::gp_device_indication(&params->zgpd_id)) {
                    result = "Completed successfully";
                } else {
                    result = "Commissioning failed (indication)";
                }
                break;
            case ZB_ZGP_COMMISSIONING_FAILED:
                result = "Commissioning failed (connection parameters)";
                break;
            case ZB_ZGP_COMMISSIONING_TIMED_OUT:
                result = "Timeout";
                break;
            case ZB_ZGP_COMMISSIONING_NO_MATCH_ERROR:
                result = "No match found for the GreenPower device in the Mapping Table";
                break;
            case ZB_ZGP_COMMISSIONING_INTERNAL_ERROR:
                result = "Internal error occurred in GP Stack";
                break;
            case ZB_ZGP_COMMISSIONING_EXTERNAL_ERROR:
                result = "External error has occurred";
                break;
            case ZB_ZGP_COMMISSIONING_CANCELLED_BY_USER:
                result = "User cancelled commissioning";
                break;
            case ZB_ZGP_ZGPD_DECOMMISSIONED:
                result = "Device sent Decommissioning command";
                break;
            default:
                result = "Unknown";
                break;
            }
            ESP_LOGD(TAG, "ZB_ZGP_SIGNAL_COMMISSIONING app_id: %d, src_id: %08lx, endpoint: %d, result: %s", params->zgpd_id.app_id, params->zgpd_id.addr.src_id, params->zgpd_id.endpoint, result);
        }
        break;
    case ZB_COMMON_SIGNAL_CAN_SLEEP:
        ESP_LOGD(TAG, "ZB_COMMON_SIGNAL_CAN_SLEEP");
        break;
    case ZB_ZDO_SIGNAL_PRODUCTION_CONFIG_READY:
        ESP_LOGD(TAG, "ZB_ZDO_SIGNAL_PRODUCTION_CONFIG_READY");
        break;
    case ZB_NWK_SIGNAL_NO_ACTIVE_LINKS_LEFT:
        ESP_LOGD(TAG, "ZB_NWK_SIGNAL_NO_ACTIVE_LINKS_LEFT");
        break;
    case ZB_ZDO_SIGNAL_DEVICE_AUTHORIZED:
        {
            static_assert(sizeof(zb_zdo_signal_device_authorized_params_t) == 12);
            auto parameters = ZB_ZDO_SIGNAL_GET_PARAMS(sg_p, zb_zdo_signal_device_authorized_params_t);
            ESP_LOGD(TAG, "ZB_ZDO_SIGNAL_DEVICE_AUTHORIZED long_addr: " IEEE_ADDR_FMT ", short_addr: %04x, auth_type: %d, auth_status: %d = %s",
                IEEE_ADDR_PRINT(parameters->long_addr), parameters->short_addr, parameters->authorization_type, parameters->authorization_status, success ? "success" : "failed");
            zb_ncp::indication(ZDO_DEV_AUTHORIZED_IND, parameters, sizeof(zb_zdo_signal_device_authorized_params_t));
        }
        break;
    case ZB_ZDO_SIGNAL_DEVICE_UPDATE:
        {
            static_assert(sizeof(zb_zdo_signal_device_update_params_t) == 14);
            auto parameters = ZB_ZDO_SIGNAL_GET_PARAMS(sg_p, const zb_zdo_signal_device_update_params_t);
            ESP_LOGD(TAG, "ZB_ZDO_SIGNAL_DEVICE_UPDATE short_addr: %04x, parent: %04x, status: %d = %s", parameters->short_addr, parameters->parent_short, parameters->status, success ? "success" : "failed");
            zb_ncp::indication(ZDO_DEV_UPDATE_IND, parameters, sizeof(zb_zdo_signal_device_update_params_t) - sizeof(parameters->tc_action) - sizeof(parameters->parent_short));
        }
        break;
    case ZB_NLME_STATUS_INDICATION:
        {
            auto parameters = ZB_ZDO_SIGNAL_GET_PARAMS(sg_p, const zb_zdo_signal_nlme_status_indication_params_t);
            ESP_LOGE(TAG, "ZB_NLME_STATUS_INDICATION. NWK: %04x, Status: %s(%d)", parameters->nlme_status.network_addr,
                utils::get_nlme_status_str(parameters->nlme_status.status), parameters->nlme_status.status);
            if (parameters->nlme_status.status == ZB_NWK_COMMAND_STATUS_UNKNOWN_COMMAND) {
                ESP_LOGE(TAG, "ZB_NLME_STATUS_INDICATION. Unknown command: %04x", parameters->nlme_status.unknown_command_id);
            }
        }
        break;
    case ZB_NWK_SIGNAL_PERMIT_JOIN_STATUS:
        if (success) {
            auto parameters = ZB_ZDO_SIGNAL_GET_PARAMS(sg_p, const uint8_t);
            if (*parameters) {
                ESP_LOGI(TAG, "Network(0x%04hx) is open for %d seconds", zb_get_pan_id(), *parameters);
            } else {
                ESP_LOGI(TAG, "Network(0x%04hx) closed, devices joining not allowed.", zb_get_pan_id());
            }
        } else {
            ESP_LOGE(TAG, "ZB_NWK_SIGNAL_PERMIT_JOIN_STATUS failed (error: %d)", ERROR_GET_CODE(status));
        }
        break;
    case ZB_ZGP_SIGNAL_MODE_CHANGE:
        {
            auto parameters = ZB_ZDO_SIGNAL_GET_PARAMS(sg_p, const zb_zgp_signal_mode_change_params_t);
            const char* reason;
            switch (parameters->reason) {
            case ZB_ZGP_MODE_CHANGE_TRIGGERED_BY_COMMAND:
                reason = "Commissioning Mode Command";
                break;
            case ZB_ZGP_MODE_CHANGE_TRIGGERED_BY_USER:
                reason = "User set the Mode";
                break;
            case ZB_ZGP_MODE_CHANGE_ON_FIRST_PARING_EXIT:
                reason = "New Device has been joined";
                break;
            case ZB_ZGP_MODE_CHANGE_TIMEOUT:
                reason = "Commissioning timeout expiration";
                break;
            default:
                reason = "Unknown";
                break;
            }
            ESP_LOGI(TAG, "Green Power Network in %s, Reason: %s", parameters->new_mode == ZB_ZGP_OPERATIONAL_MODE ? "operational mode" : "commissioning mode", reason);
        }
        break;
    case ZB_ZDO_DEVICE_UNAVAILABLE:
        {
            auto parameters = ZB_ZDO_SIGNAL_GET_PARAMS(sg_p, const zb_zdo_device_unavailable_params_t);
            ESP_LOGD(TAG, "ZB_ZDO_DEVICE_UNAVAILABLE long_addr: " IEEE_ADDR_FMT ", short_addr: %04x", IEEE_ADDR_PRINT(parameters->long_addr), parameters->short_addr);
        }
        break;
    case ZB_ZGP_SIGNAL_APPROVE_COMMISSIONING:
        {
            auto params = ZB_ZDO_SIGNAL_GET_PARAMS(sg_p, const zb_zgp_signal_approve_comm_params_t)->params;
            ESP_LOGD(TAG, "ZB_ZGP_SIGNAL_APPROVE_COMMISSIONING: device_id: 0x%02x, gp_id.app_id: 0x%02x, gp_id.endpoint: %d, gp_id.src_id: %08lx, pairing_endpoint: %d, pairing_configuration: %s, actions: 0x%02x, num_of_endpoints: %u",
                params->device_id, params->zgpd_id.app_id, params->zgpd_id.endpoint, params->zgpd_id.addr.src_id, params->pairing_endpoint, params->pairing_configuration ? "true" : "false", params->actions, params->num_of_endpoints
            );
            if (zb_ncp::add_gp_device_to_mapping_table(&params->zgpd_id) && zb_ncp::gp_device_commissioning(&params->zgpd_id, params->device_id, params->manuf_id, params->manuf_model_id, params->ieee_addr)) {
                zb_zgps_accept_commissioning(ZB_TRUE);
            } else {
                zb_zgps_accept_commissioning(ZB_FALSE);
            }
        }
        break;
    default:
        ESP_LOGE(TAG, "Unknown Signal: %d, Status %d", sig, ERROR_GET_CODE(status));
        break;
    }
    zb_buf_free(param);
}

bool zb_zcl_green_power_cluster_handler(zb_uint8_t param) {
    zb_ret_t ret = zb_buf_get_status(param);
    if (unlikely(ret != RET_OK)) {
        ESP_LOGE(TAG, "GreenPower PROXY Indication failed (error: %d)", ERROR_GET_CODE(ret));
        return false;
    }
    auto len = zb_buf_len(param);
    auto buf_ptr = static_cast<uint8_t*>(zb_buf_begin(param));
    auto ind = ZB_BUF_GET_PARAM(param, zb_zcl_parsed_hdr_t);
    zb_buf_begin(param);
    if (unlikely(!buf_ptr || !ind || len == 0 )) {
        ESP_LOGE(TAG, "GreenPower PROXY Indication with zero length");
        return false;
    }
    auto in_opt = reinterpret_cast<zb_uint16_t*>(buf_ptr);
    uint8_t app_id = ZB_ZGP_GP_NOTIF_OPT_GET_APP_ID(*in_opt);
    GreenPowerParser gp_buf(app_id, buf_ptr);
    zb_zgpd_id_t zgpd_id = gp_buf.get_zgpd_id();
    int t_idx = zb_ncp::get_gp_device_mapping_table_idx(&zgpd_id);
    if (unlikely(t_idx < 0)) {
        ESP_LOGE(TAG, "PROXY: GreenPower Device with src_id: %08lx not found in Mapping Table", zgpd_id.addr.src_id);
        return false;
    }
    auto t_entry = &zb_ncp::d_mapping_table[t_idx];
    if (ind->cmd_id == ZGP_SERVER_CMD_GP_COMMISSIONING_NOTIFICATION) {
        auto p_hdr = reinterpret_cast<zb_ncp::aps_data_ind_header_t*>(t_entry->payload);
        if (p_hdr->dataLength == 0) {
            ESP_LOGD(TAG, "PROXY: GreenPower Indication has already been sent");
            return true;
        }
        memset(t_entry->payload, 0, sizeof(t_entry->payload));
        t_entry->sec_frame_counter = 0;
    } else {
        auto sec_counter = gp_buf.get_sec_frame_counter();
        if (t_entry->sec_frame_counter == sec_counter) {
            ESP_LOGD(TAG, "PROXY: GreenPower Indication has already been sent");
            return true;
        }
        t_entry->sec_frame_counter = sec_counter;
    }
    auto gp_proxy = reinterpret_cast<zb_zgp_gp_proxy_info_t*>(gp_buf.get_end_ptr() + gp_buf.get_payload_len());
    uint8_t zcl_hdr[5];
    uint8_t zcl_hdr_len = 0;
    uint8_t zcl_hdr_fc = ZB_ZCL_CONSTRUCT_FRAME_CONTROL(
        ind->is_common_command ? ZB_ZCL_FRAME_TYPE_COMMON : ZB_ZCL_FRAME_TYPE_CLUSTER_SPECIFIC,
        static_cast<uint8_t>(ind->is_manuf_specific ? 1 : 0),
        ind->cmd_direction,
        ind->disable_default_response ? ZB_ZCL_DISABLE_DEFAULT_RESPONSE : ZB_ZCL_ENABLE_DEFAULT_RESPONSE
    );
    zcl_hdr[zcl_hdr_len++] = zcl_hdr_fc;
    if (unlikely(ind->is_manuf_specific)) {
        memcpy(&zcl_hdr[zcl_hdr_len], &ind->manuf_specific, sizeof(ind->manuf_specific));
        zcl_hdr_len += sizeof(ind->manuf_specific);
    }
    zcl_hdr[zcl_hdr_len++] = ind->seq_number;
    zcl_hdr[zcl_hdr_len++] = ind->cmd_id;
    uint8_t outdata[sizeof(zb_ncp::aps_data_ind_header_t) + zcl_hdr_len + len];
    zb_uint16_t mac_dst_addr = 0x0000;
    auto hdr = reinterpret_cast<zb_ncp::aps_data_ind_header_t*>(outdata);
    hdr->paramLength = sizeof(zb_ncp::aps_data_ind_header_t) - sizeof(hdr->paramLength) - sizeof(hdr->dataLength);
    hdr->dataLength = zcl_hdr_len + len;
    hdr->apsFC = ind->addr_data.common_data.fc;
    hdr->srcNwk = gp_proxy->short_addr;
    hdr->dstNwk = ind->addr_data.common_data.dst_addr;
    hdr->grpNwk = mac_dst_addr;
    hdr->dstEndpoint = ind->addr_data.common_data.dst_endpoint;
    hdr->srcEndpoint = ind->addr_data.common_data.src_endpoint;
    hdr->clusterID = ind->cluster_id;
    hdr->profileID = ind->profile_id;
    hdr->apsCounter = ind->seq_number;
    hdr->srcMAC = gp_proxy->short_addr;
    hdr->dstMAC = mac_dst_addr;
    hdr->lqi = gp_proxy->link;
    hdr->rssi = (uint8_t)ind->rssi;
#if (defined ZB_ENABLE_SE) || (defined ZB_ZCL_SUPPORT_CLUSTER_WWAH)
    hdr->apsKey = ind->addr_data.common_data.aps_key_source | (ind->addr_data.common_data.aps_key_attrs << 1) | (ind->addr_data.common_data.aps_key_from_tc << 3) | (ind->addr_data.common_data.reserved << 4);
#else
    const uint8_t zb_sec = ZGP_GPS_GET_SECURITY_LEVEL(); // zb_zgps_get_security_level();
    hdr->apsKey = (zb_sec & 0x03) | ((zb_sec >> 2U) << 1) | ((zb_sec >> 3U) << 3) | (0U << 4);
#endif
    memcpy(&outdata[sizeof(zb_ncp::aps_data_ind_header_t)], zcl_hdr, zcl_hdr_len);
    memcpy(&outdata[sizeof(zb_ncp::aps_data_ind_header_t) + zcl_hdr_len], gp_buf.get_ptr(), len);
    auto opt = gp_buf.get_options();
    if ((opt >> 6) & 0x03) {
        uint16_t clean_options = opt & ~0x00C0;
        memcpy(&outdata[sizeof(zb_ncp::aps_data_ind_header_t) + zcl_hdr_len], &clean_options, sizeof(clean_options));
    }
    ESP_LOGD(TAG, "GreenPower PROXY Indication: profileId: %04x, clusterId: %04x, srcAddr: %04x, dstAddr: %04x, groupAddr: %04x, srcEndpoint: %u, dstEndpoint: %u, mac_src_addr: %04x, mac_dst_addr: %04x, fc: 0x%02x, lqi: %u, rssi: %d, TSN: %u, dataLen: %u",
        hdr->profileID, hdr->clusterID, hdr->srcNwk, hdr->dstNwk, hdr->grpNwk, hdr->srcEndpoint, hdr->dstEndpoint, hdr->srcMAC, hdr->dstMAC, hdr->apsFC, hdr->lqi, ind->rssi, ind->seq_number, hdr->dataLength
    );
    zb_ncp::indication(APSDE_DATA_IND, outdata, sizeof(outdata));
    return true;
}

void zb_zgp_gpdf_raw_indication(zb_bufid_t buf_ref) {
    zb_ret_t ret = zb_buf_get_status(buf_ref);
    if (unlikely(ret != RET_OK)) {
        ESP_LOGE(TAG, "GreenPower RAW Indication failed (error: %d)", ERROR_GET_CODE(ret));
        return;
    }
    uint8_t buf_len = zb_buf_len(buf_ref);
    auto ind = ZB_BUF_GET_PARAM(buf_ref, zb_gpdf_info_t);
    int t_idx = zb_ncp::get_gp_device_mapping_table_idx(&ind->zgpd_id);
    if (unlikely(t_idx < 0)) {
        ESP_LOGE(TAG, "RAW: GreenPower Device with src_id: %08lx not found in Mapping Table", ind->zgpd_id.addr.src_id);
        return;
    }
    auto t_entry = &zb_ncp::d_mapping_table[t_idx];
    if (t_entry->sec_frame_counter == ind->sec_frame_counter) {
        ESP_LOGD(TAG, "RAW: GreenPower Indication has already been sent");
        return;
    }
    t_entry->sec_frame_counter = ind->sec_frame_counter;
    zgp_tbl_ent_t entry;
    ret = zgp_sink_table_read(&ind->zgpd_id, &entry);
    if (unlikely(ret != RET_OK)) {
        ESP_LOGE(TAG, "GreenPower RAW Indication: Failed to read sink table entry (error: %d)", ERROR_GET_CODE(ret));
        return;
    }
    uint8_t sec_level, rx_after_tx, cmd_direction;
    if (ZB_GPDF_NFC_GET_NFC_EXT(ind->nwk_frame_ctl) > 0) {
        sec_level = ZB_GPDF_EXT_NFC_GET_SEC_LEVEL(ind->nwk_ext_frame_ctl);
        // key_type = ZB_GPDF_EXT_NFC_GET_SEC_KEY(ind->nwk_ext_frame_ctl);
        rx_after_tx = ZB_GPDF_EXT_NFC_GET_RX_AFTER_TX(ind->nwk_ext_frame_ctl);
        cmd_direction = ZB_GPDF_EXT_NFC_GET_DIRECTION(ind->nwk_ext_frame_ctl);
    } else {
        sec_level = ZB_ZGP_GP_NOTIF_OPT_GET_SEC_LVL(entry.options);
        // key_type = ZB_ZGP_GP_NOTIF_OPT_GET_KEY_TYPE(entry.options);
        rx_after_tx = ZB_ZGP_GP_NOTIF_OPT_GET_RX_AFTER_TX(entry.options);
        cmd_direction = ZGP_FRAME_DIR_FROM_ZGPD;
    }
    uint8_t bidir_cap = ZB_ZGP_GP_NOTIF_OPT_GET_BIDIR_CAP(entry.options);
    uint8_t is_proxy = ZB_ZGP_GP_NOTIF_OPT_GET_PROXY_INFO_PRESENT(entry.options);
    uint8_t is_manuf_specific = (entry.u.sink.device_id == 0xFE || entry.u.sink.device_id == 0xFF) ? 1 : 0;
    uint8_t zcl_hdr[5];
    uint8_t zcl_hdr_len = 0;
    uint8_t zcl_hdr_fc = ZB_ZCL_CONSTRUCT_FRAME_CONTROL(
        ZB_ZCL_FRAME_TYPE_CLUSTER_SPECIFIC,
        is_manuf_specific,
        cmd_direction,
        ZB_ZCL_DISABLE_DEFAULT_RESPONSE
    );
    zcl_hdr[zcl_hdr_len++] = zcl_hdr_fc;
    if (unlikely(is_manuf_specific == 1)) {
        zb_uint16_t manuf_id = ZGP_CTXC().comm_data.app_info.manuf_id; // or save to your custom table instead
        memcpy(&zcl_hdr[zcl_hdr_len], &manuf_id, sizeof(manuf_id));
        zcl_hdr_len += sizeof(manuf_id);
    }
    zcl_hdr[zcl_hdr_len++] = ind->mac_seq_num;
    zcl_hdr[zcl_hdr_len++] = ZGP_SERVER_CMD_GP_NOTIFICATION;
    zb_uint16_t dst_nwk;
    if (ind->mac_addr_flds_len == 2 || ind->mac_addr_flds_len == 4) {
        dst_nwk = ind->mac_addr_flds.s.dst_addr;
    } else if (ind->mac_addr_flds_len == 8) {
        dst_nwk = zb_address_short_by_ieee(ind->mac_addr_flds.l.addr);
    } else {
        dst_nwk = ind->mac_addr_flds.comb.dst_addr;
    }
    zb_uint16_t grp_nwk = 0x0000;
    uint8_t outdata[sizeof(zb_ncp::aps_data_ind_header_t) + zcl_hdr_len + GreenPowerParser::MAX_SIZE + buf_len];
    GreenPowerParser gp_buf(ind->zgpd_id.app_id, &outdata[sizeof(zb_ncp::aps_data_ind_header_t) + zcl_hdr_len]);
    auto hdr = reinterpret_cast<zb_ncp::aps_data_ind_header_t*>(outdata);
    hdr->paramLength = sizeof(zb_ncp::aps_data_ind_header_t) - sizeof(hdr->paramLength) - sizeof(hdr->dataLength);
    hdr->dataLength = zcl_hdr_len + gp_buf.size() + buf_len;
    hdr->apsFC = ind->nwk_frame_ctl;
    hdr->srcNwk = entry.zgpd_assigned_alias;
    hdr->dstNwk = dst_nwk;
    hdr->grpNwk = grp_nwk;
    hdr->dstEndpoint = (ind->zgpd_id.endpoint == 0) ? ZGP_ENDPOINT : ind->zgpd_id.endpoint;
    hdr->srcEndpoint = ZGP_ENDPOINT;
    hdr->clusterID = ZB_ZCL_CLUSTER_ID_GREEN_POWER;
    hdr->profileID = ZB_AF_GP_PROFILE_ID;
    hdr->apsCounter = ind->mac_seq_num;
    hdr->srcMAC = entry.zgpd_assigned_alias;
    hdr->dstMAC = grp_nwk;
    hdr->lqi = ind->lqi;
    hdr->rssi = (uint8_t)ind->rssi;
    const uint8_t zb_sec = ZGP_GPS_GET_SECURITY_LEVEL(); // zb_zgps_get_security_level();
    hdr->apsKey = (zb_sec & 0x03) | ((zb_sec >> 2U) << 1) | ((zb_sec >> 3U) << 3) | (0U << 4);
    memcpy(&outdata[sizeof(zb_ncp::aps_data_ind_header_t)], zcl_hdr, zcl_hdr_len);
    uint16_t notif_opt = ZB_ZGP_FILL_COMM_NOTIFICATION_OPTIONS(
        ind->zgpd_id.app_id,
        rx_after_tx,
        sec_level,
        ind->key_type,
        (ind->status != 1 ? 1 : 0),
        bidir_cap,
        is_proxy
    );
    gp_buf.set_options(notif_opt);
    gp_buf.set_src_id(ind->zgpd_id.addr.src_id);
    gp_buf.set_endpoint(ind->zgpd_id.endpoint);
    gp_buf.set_ieee_addr(ind->zgpd_id.addr.ieee_addr);
    gp_buf.set_sec_frame_counter(ind->sec_frame_counter);
    gp_buf.set_zgpd_cmd_id(ind->zgpd_cmd_id);
    gp_buf.set_payload_len(buf_len);
    if (unlikely(buf_len > 0)) {
        memcpy(gp_buf.get_end_ptr(), zb_buf_begin(buf_ref), buf_len);
    }
    ESP_LOGD(TAG, "GreenPower SINK Indication: profileId: %04x, clusterId: %04x, srcAddr: %04x, dstAddr: %04x, groupAddr: %04x, srcEndpoint: %u, dstEndpoint: %u, mac_src_addr: %04x, mac_dst_addr: %04x, fc: 0x%02x, options: 0x%04x, lqi: %u, rssi: %d, TSN: %u, dataLen: %u",
        hdr->profileID, hdr->clusterID, hdr->srcNwk, hdr->dstNwk, hdr->grpNwk, hdr->srcEndpoint, hdr->dstEndpoint, hdr->srcMAC, hdr->dstMAC, hdr->apsFC, notif_opt, hdr->lqi, ind->rssi, ind->mac_seq_num, hdr->dataLength
    );
    zb_ncp::indication(APSDE_DATA_IND, outdata, sizeof(zb_ncp::aps_data_ind_header_t) + hdr->dataLength);
    zb_buf_free(buf_ref);
}

zb_uint8_t zcl_specific_cluster_cmd_handler(zb_uint8_t param) {
    auto len = zb_buf_len(param);
    auto ind = ZB_BUF_GET_PARAM(param, zb_zcl_parsed_hdr_t);
    ESP_LOGW(TAG, "zcl_specific_cluster_cmd_handler(): zb_buf_len: %d", len);
    ESP_LOG_BUFFER_HEX_LEVEL(TAG, ind, len, ESP_LOG_WARN);
    // TO DO
    // zb_buf_free(param);
    return ZB_FALSE; // ZB_TRUE;
}
