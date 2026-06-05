#include "commands_helpers.h"
#include "utils.h"
#include "app.h"
#include <esp_mac.h>

template <> struct zb_ncp::cmd_handle<GET_JOINED> :
    immediate_cmd_process<GET_JOINED>,
    general_status_res<GET_JOINED, uint8_t> {
    static void process_status_res(ncp_generic_status_t& status, uint8_t* res) {
        *res = ZB_JOINED() ? 1 : 0;
    }
};

template <> struct zb_ncp::cmd_handle<GET_ZIGBEE_ROLE> :
    immediate_cmd_process<GET_ZIGBEE_ROLE>,
    general_status_res<GET_ZIGBEE_ROLE, uint8_t> {
    static void process_status_res(ncp_generic_status_t& status, uint8_t* res) {
        *res = zb_get_network_role();
        if (*res != ZB_NWK_DEVICE_TYPE_COORDINATOR) {
            ESP_LOGI(TAG, "Device is not a ZB_Coordinator, current role: %s", *res == ZB_NWK_DEVICE_TYPE_NONE ? "Unknown Device" : (*res == ZB_NWK_DEVICE_TYPE_ROUTER ? "Router" : "End Device"));
            *res = ZB_NWK_DEVICE_TYPE_COORDINATOR;
        }
    }
};

struct LOCAL_IEEE_ADDR_t {
    uint8_t mac;
    zb_ieee_addr_t ieee;
} __attribute__((packed)) __attribute__((aligned(1)));
template <> struct zb_ncp::cmd_handle<GET_LOCAL_IEEE_ADDR> :
    immediate_cmd_process<GET_LOCAL_IEEE_ADDR>,
    general_status_arg_res<GET_LOCAL_IEEE_ADDR, uint8_t, LOCAL_IEEE_ADDR_t> {
    static void process_status_arg_res(ncp_generic_status_t& status, uint8_t arg, LOCAL_IEEE_ADDR_t* res) {
        if (arg != 0) {
            ESP_LOGE(TAG, "Invalid MAC address: %d", arg);
            status = GENERIC_INVALID_PARAMETER_1;
        } else {
            res->mac = arg;
            zb_ieee_addr_t ieee_addr;
            zb_get_long_address(ieee_addr);
            memcpy(res->ieee, ieee_addr, sizeof(zb_ieee_addr_t));
            ESP_LOGD(TAG, "Current local IEEE addr: " IEEE_ADDR_FMT, IEEE_ADDR_PRINT(ieee_addr));
        }
    }
};

template <> struct zb_ncp::cmd_handle<SET_LOCAL_IEEE_ADDR> :
    immediate_cmd_process<SET_LOCAL_IEEE_ADDR>,
    general_status_arg<SET_LOCAL_IEEE_ADDR, LOCAL_IEEE_ADDR_t> {
    static void process_status_arg(ncp_generic_status_t& status, const LOCAL_IEEE_ADDR_t& arg) {
        status = GENERIC_BLOCKED;
    }
};

template <> struct zb_ncp::cmd_handle<GET_EXTENDED_PAN_ID> :
    immediate_cmd_process<GET_EXTENDED_PAN_ID>,
    general_status_res<GET_EXTENDED_PAN_ID, zb_ext_pan_id_t> {
    static void process_status_res(ncp_generic_status_t& status, zb_ext_pan_id_t* res) {
        zb_ext_pan_id_t ext_pan_id;
        zb_get_extended_pan_id(ext_pan_id);
        ESP_LOGD(TAG, "Current extended PanID: " IEEE_ADDR_FMT, IEEE_ADDR_PRINT(ext_pan_id));
        utils::reverse_ieee_addr(ext_pan_id);
        memcpy(res, ext_pan_id, sizeof(zb_ext_pan_id_t));
    }
};

template <> struct zb_ncp::cmd_handle<SET_EXTENDED_PAN_ID> :
    immediate_cmd_process<SET_EXTENDED_PAN_ID>,
    general_status_arg<SET_EXTENDED_PAN_ID, zb_ext_pan_id_t> {
    static void process_status_arg(ncp_generic_status_t& status, const zb_ext_pan_id_t& arg) {
        zb_ext_pan_id_t ext_pan_id;
        memcpy(ext_pan_id, arg, sizeof(zb_ext_pan_id_t));
        zb_set_extended_pan_id(ext_pan_id);
    }
};

template <> struct zb_ncp::cmd_handle<GET_PAN_ID> :
    immediate_cmd_process<GET_PAN_ID>,
    general_status_res<GET_PAN_ID, uint16_t> {
    static void process_status_res(ncp_generic_status_t& status, unalign_uint16_t* res) {
        *res = storage::app_config().pan_id; // zb_get_pan_id();
        ESP_LOGD(TAG, "Current PanID: 0x%04hx", *res);
    }
};

template <> struct zb_ncp::cmd_handle<SET_PAN_ID> :
    immediate_cmd_process<SET_PAN_ID>,
    general_status_arg<SET_PAN_ID, uint16_t> {
    static void process_status_arg(ncp_generic_status_t& status, uint16_t arg) {
        zb_set_pan_id(arg);
        if (storage::app_config().pan_id != arg) {
            esp_err_t ret = storage::write_param(offsetof(storage::zb_app_config_t, pan_id), &arg, sizeof(arg));
            if (ret == ESP_OK) {
                storage::app_config().pan_id = arg;
            } else {
                ESP_LOGE(TAG, "Failed to write SET_PAN_ID to LittleFS Partition (error: %s)", esp_err_to_name(ret));
            }
        }
    }
};

struct GET_ZIGBEE_CHANNEL_resp_t {
    uint8_t page;
    uint8_t channel;
} __attribute__((packed)) __attribute__((aligned(1)));
template <> struct zb_ncp::cmd_handle<GET_ZIGBEE_CHANNEL> :
    immediate_cmd_process<GET_ZIGBEE_CHANNEL>,
    general_status_res<GET_ZIGBEE_CHANNEL, GET_ZIGBEE_CHANNEL_resp_t> {
    static void process_status_res(ncp_generic_status_t& status, GET_ZIGBEE_CHANNEL_resp_t* res) {
        res->page = 0;
        res->channel = zb_get_current_channel();
        ESP_LOGD(TAG, "Current RadioChannel: %d", res->channel);
    }
};

template <> struct zb_ncp::cmd_handle<SET_ZIGBEE_ROLE> :
    immediate_cmd_process<SET_ZIGBEE_ROLE>,
    general_status_arg<SET_ZIGBEE_ROLE, uint8_t> {
    static void process_status_arg(ncp_generic_status_t& status, uint8_t role) {
        if (role != ZB_NWK_DEVICE_TYPE_COORDINATOR) {
            status = GENERIC_INVALID_PARAMETER_1;
        }
    }
};

struct ZIGBEE_CHANNEL_MASK_arg_t {
    uint8_t page;
    uint32_t mask;
} __attribute__((packed)) __attribute__((aligned(1)));
template <> struct zb_ncp::cmd_handle<SET_ZIGBEE_CHANNEL_MASK> :
    immediate_cmd_process<SET_ZIGBEE_CHANNEL_MASK>,
    general_status_arg<SET_ZIGBEE_CHANNEL_MASK, ZIGBEE_CHANNEL_MASK_arg_t> {
    static void process_status_arg(ncp_generic_status_t& status, const ZIGBEE_CHANNEL_MASK_arg_t& arg) {
        if (arg.page != 0) {
            status = GENERIC_INVALID_PARAMETER_1;
        } else {
            zb_ncp::set_channel_mask(arg.mask);
        }
    }
};

struct GET_ZIGBEE_CHANNEL_MASK_resp_t {
    uint8_t len;
    uint8_t page;
    uint32_t mask;
} __attribute__((packed)) __attribute__((aligned(1)));
template <> struct zb_ncp::cmd_handle<GET_ZIGBEE_CHANNEL_MASK> :
    immediate_cmd_process<GET_ZIGBEE_CHANNEL_MASK>,
    general_status_res<GET_ZIGBEE_CHANNEL_MASK, GET_ZIGBEE_CHANNEL_MASK_resp_t> {
    static void process_status_res(ncp_generic_status_t& status, GET_ZIGBEE_CHANNEL_MASK_resp_t* res) {
        res->len = 1;
        res->page = 0;
        res->mask = zb_get_channel_mask();
    }
};

struct SET_NWK_KEY_arg_t {
    uint8_t nwkKey[16];
    uint8_t index;
} __attribute__((packed)) __attribute__((aligned(1)));
template <> struct zb_ncp::cmd_handle<SET_NWK_KEY> :
    immediate_cmd_process<SET_NWK_KEY>,
    general_status_arg<SET_NWK_KEY, SET_NWK_KEY_arg_t> {
    static void process_status_arg(ncp_generic_status_t& status, const SET_NWK_KEY_arg_t& arg) {
        if (arg.index > 3) {
            status = GENERIC_INVALID_PARAMETER_2;
            return;
        }
        zb_secur_setup_nwk_key(const_cast<zb_uint8_t*>(arg.nwkKey), arg.index); // esp_zb_secur_TC_standard_preconfigure_key_set(const_cast<zb_uint8_t*>(arg.nwkKey));
    }
};

struct NWK_FORMATION_arg_header_t {
    uint8_t len; // amount of channels
    ZIGBEE_CHANNEL_MASK_arg_t channels[];
} __attribute__((packed)) __attribute__((aligned(1)));
struct NWK_FORMATION_arg_tail_t {
    uint8_t duration;
    uint8_t distribFlag;
    uint16_t distribNwk;
    zb_ext_pan_id_t extendedPanID;
} __attribute__((packed)) __attribute__((aligned(1)));
template <> struct zb_ncp::cmd_handle<NWK_FORMATION> :
    delayed_cmd_process<NWK_FORMATION, single_cmd_delayed> {
    static constexpr uint16_t resp_buffer_size = sizeof(generic_response_t) + sizeof(uint16_t);
    static constexpr const char* name = "NWK_FORMATION";
    static ncp_generic_status_t start_delayed(const void* inbuffer, uint16_t inlen) {
        if (!inbuffer || inlen < (sizeof(NWK_FORMATION_arg_header_t) + sizeof(NWK_FORMATION_arg_tail_t))) {
            return GENERIC_INVALID_PARAMETER;
        }
        auto hdr = static_cast<const NWK_FORMATION_arg_header_t*>(inbuffer);
        uint16_t channels_size = hdr->len * sizeof(ZIGBEE_CHANNEL_MASK_arg_t);
        if (inlen < (sizeof(NWK_FORMATION_arg_header_t) + channels_size + sizeof(NWK_FORMATION_arg_tail_t))) {
            return GENERIC_INVALID_PARAMETER;
        }
        for (uint8_t i = 0; i < hdr->len; ++i) {
            if (hdr->channels[i].page != 0) return GENERIC_INVALID_PARAMETER;
            zb_ncp::set_channel_mask(hdr->channels[i].mask);
            ESP_LOGD(TAG, "Set Channel Mask: %08lx", hdr->channels[i].mask);
        }
        const uint8_t* tail_ptr = reinterpret_cast<const uint8_t*>(hdr->channels) + channels_size;
        auto tail = reinterpret_cast<const NWK_FORMATION_arg_tail_t*>(tail_ptr);
        zb_ext_pan_id_t ext_pan_id;
        memcpy(ext_pan_id, tail->extendedPanID, sizeof(zb_ext_pan_id_t));
        zb_set_extended_pan_id(ext_pan_id);
        ESP_LOGD(TAG, "Set extended PanID. NWK: %04x, Flags: %d, Duration: %d, NewPanID: " IEEE_ADDR_FMT,
            tail->distribNwk, tail->distribFlag, tail->duration, IEEE_ADDR_PRINT(ext_pan_id));
        if (!zb_ncp::start_zigbee_stack()) {
            if (ZB_SCHEDULE_APP_CALLBACK(commissioning_callback, ZB_BDB_NETWORK_FORMATION) != RET_OK) {
                ESP_LOGE(TAG, "Commissioning failed: out of memory");
                return GENERIC_ERROR;
            }
        }
        return GENERIC_OK;
    }
    static uint16_t finish_delayed(ncp_generic_status_t status, uint8_t* outdata, uint16_t outlen) {
        auto full_resp = reinterpret_cast<generic_response_t*>(outdata);
        full_resp->category = STATUS_CATEGORY_NWK;
        full_resp->status = status;
        *reinterpret_cast<unalign_uint16_t*>(full_resp + 1) = 0; // (status == GENERIC_OK) ? zb_get_pan_id() : 0;
        return sizeof(generic_response_t) + sizeof(uint16_t);
    }
};
SINGLE_CMD_DELAYED_DECL(NWK_FORMATION)

template <> struct zb_ncp::cmd_handle<NWK_START_WITHOUT_FORMATION> :
    delayed_cmd_process<NWK_START_WITHOUT_FORMATION, single_cmd_delayed> {
    static constexpr uint16_t resp_buffer_size = sizeof(generic_response_t);
    static constexpr const char* name = "NWK_START_WITHOUT_FORMATION";
    static ncp_generic_status_t start_delayed(const void* inbuffer, uint16_t inlen) {
        if (!zb_ncp::start_zigbee_stack()) {
            response(GENERIC_OK);
        }
        return GENERIC_OK;
    }
    static uint16_t finish_delayed(ncp_generic_status_t status, uint8_t* outdata, uint16_t outlen) {
        auto full_resp = reinterpret_cast<generic_response_t*>(outdata);
        full_resp->category = STATUS_CATEGORY_NWK;
        full_resp->status = status;
        return sizeof(generic_response_t);
    }
};
SINGLE_CMD_DELAYED_DECL(NWK_START_WITHOUT_FORMATION)

template <> struct zb_ncp::cmd_handle<ZDO_SET_NODE_DESC_MANUF_CODE> :
    delayed_cmd_process<ZDO_SET_NODE_DESC_MANUF_CODE, single_cmd_delayed> {
    static constexpr uint16_t resp_buffer_size = sizeof(generic_response_t);
    static constexpr const char* name = "ZDO_SET_NODE_DESC_MANUF_CODE";
    static void set_manufacturer_code_callback(zb_ret_t status) {
        if (Cmd::need_resolve()) {
            Cmd::response(status == RET_OK ? GENERIC_OK : static_cast<ncp_generic_status_t>(ERROR_GET_CODE(status)));
        }
    }
    static ncp_generic_status_t start_delayed(const void* inbuffer, uint16_t inlen) {
        auto arg = *static_cast<const uint16_t*>(inbuffer);
        zb_set_node_descriptor_manufacturer_code_req(arg, Cmd::set_manufacturer_code_callback);
        ESP_LOGI(TAG, "ZDO_SET_NODE_DESC_MANUF_CODE: 0x%04X", arg);
        return GENERIC_OK;
    }
    static uint16_t finish_delayed(ncp_generic_status_t status, uint8_t* outdata, uint16_t outlen) {
        auto full_resp = reinterpret_cast<generic_response_t*>(outdata);
        full_resp->category = STATUS_CATEGORY_ZDO;
        full_resp->status = status;
        return sizeof(generic_response_t);
    }
};
SINGLE_CMD_DELAYED_DECL(ZDO_SET_NODE_DESC_MANUF_CODE)

enum policy_type_t : unalign_uint16_t {
    LINK_KEY_REQUIRED = 0,
    IC_REQUIRED = 1,
    TC_REJOIN_ENABLED = 2,
    IGNORE_TC_REJOIN = 3,
    APS_INSECURE_JOIN = 4,
    DISABLE_NWK_MGMT_CHANNEL_UPDATE = 5
};
struct SET_TC_POLICY_arg_t {
    policy_type_t type;
    uint8_t value;
} __attribute__((packed)) __attribute__((aligned(1)));
template <> struct zb_ncp::cmd_handle<SET_TC_POLICY> :
    immediate_cmd_process<SET_TC_POLICY>,
    general_status_arg<SET_TC_POLICY, SET_TC_POLICY_arg_t> {
    static void process_status_arg(ncp_generic_status_t& status, const SET_TC_POLICY_arg_t& arg) {
        switch (arg.type) {
        case LINK_KEY_REQUIRED:
            zb_aib_tcpol_set_update_trust_center_link_keys_required(arg.value == 0 ? ZB_FALSE : ZB_TRUE); // esp_zb_secur_link_key_exchange_required_set(false);
            ESP_LOGI(TAG, "Link Key exchange required: %s", arg.value == 0 ? "FALSE" : "TRUE");
            break;
        case IC_REQUIRED:
            zb_set_installcode_policy(arg.value == 0 ? ZB_FALSE : ZB_TRUE);
            ESP_LOGI(TAG, "Install Code required: %s", arg.value == 0 ? "FALSE" : "TRUE");
            break;
        case TC_REJOIN_ENABLED:
            zb_secur_set_tc_rejoin_enabled(arg.value == 0 ? ZB_FALSE : ZB_TRUE);
            ESP_LOGI(TAG, "TC rejoin enabled: %s", arg.value == 0 ? "FALSE" : "TRUE");
            break;
        case IGNORE_TC_REJOIN:
            zb_secur_set_ignore_tc_rejoin(arg.value == 0 ? ZB_FALSE : ZB_TRUE);
            ESP_LOGI(TAG, "Ignore TC rejoin: %s", arg.value == 0 ? "FALSE" : "TRUE");
            break;
        case APS_INSECURE_JOIN:
            zb_zdo_set_aps_unsecure_join(arg.value == 0 ? ZB_FALSE : ZB_TRUE);
            ESP_LOGI(TAG, "APS unsecure join: %s", arg.value == 0 ? "FALSE" : "TRUE");
            break;
        case DISABLE_NWK_MGMT_CHANNEL_UPDATE:
            zb_zdo_disable_network_mgmt_channel_update(arg.value == 0 ? ZB_FALSE : ZB_TRUE);
            ESP_LOGI(TAG, "Disable network MGMT channel update: %s", arg.value == 0 ? "FALSE" : "TRUE");
            break;
        default:
            status = GENERIC_INVALID_PARAMETER_1;
            ESP_LOGE(TAG, "SET_TC_POLICY Error: Invalid secure policy type: %d", arg.type);
        };
    }
};

struct AF_SIMPLE_DESC_hdr_t {
    uint8_t endpoint;
    uint16_t profileID;
    uint16_t deviceID;
    uint8_t version;
    uint8_t inputClusterCount;
    uint8_t outputClusterCount;
} __attribute__((packed)) __attribute__((aligned(1)));
template <> struct zb_ncp::cmd_handle<AF_SET_SIMPLE_DESC> :
    immediate_cmd_process<AF_SET_SIMPLE_DESC> {
    static constexpr uint16_t resp_buffer_size = sizeof(generic_response_t);
    static uint16_t process_immediate(const void* inbuffer, uint16_t inlen, uint8_t* outdata, uint16_t outlen) {
        auto full_res = reinterpret_cast<generic_response_t*>(outdata);
        full_res->category = STATUS_CATEGORY_GENERIC;
        full_res->status = GENERIC_OK;
        if (!inbuffer || inlen < sizeof(AF_SIMPLE_DESC_hdr_t)) {
            full_res->status = GENERIC_INVALID_PARAMETER;
            return sizeof(generic_response_t);
        }
        auto arg = static_cast<const AF_SIMPLE_DESC_hdr_t*>(inbuffer);
        ESP_LOGD(TAG, "AF_SET_SIMPLE_DESC Endpoint: %d, ProfileID: %04x, DeviceID: %04x, Version: %d, inputClusterCount: %d, outputClusterCount: %d",
            arg->endpoint, arg->profileID, arg->deviceID, arg->version, arg->inputClusterCount, arg->outputClusterCount);
        /*
        zb_ret_t ret = zb_add_simple_descriptor(arg);
        if (ret != RET_OK) {
            full_res->status = GENERIC_ERROR;
            ESP_LOGE(TAG, "Failed to set SIMPLE_DESC (error: %d)", ERROR_GET_CODE(ret));
        }
        */
        return sizeof(generic_response_t);
    }
};

template <> struct zb_ncp::cmd_handle<GET_RX_ON_WHEN_IDLE> :
    immediate_cmd_process<GET_RX_ON_WHEN_IDLE>,
    general_status_res<GET_RX_ON_WHEN_IDLE, uint8_t> {
    static void process_status_res(ncp_generic_status_t& status, uint8_t* res) {
        *res = zb_get_rx_on_when_idle() ? 1 : 0;
    }
};

template <> struct zb_ncp::cmd_handle<SET_RX_ON_WHEN_IDLE> :
    immediate_cmd_process<SET_RX_ON_WHEN_IDLE>,
    general_status_arg<SET_RX_ON_WHEN_IDLE, uint8_t> {
    static void process_status_arg(ncp_generic_status_t& status, uint8_t arg) {
        zb_set_rx_on_when_idle(arg == 0 ? ZB_FALSE : ZB_TRUE);
    }
};

template <> struct zb_ncp::cmd_handle<SET_TX_POWER> :
    immediate_cmd_process<SET_TX_POWER>,
    general_status_arg_res<SET_TX_POWER, uint8_t, uint8_t> {
    static void process_status_arg_res(ncp_generic_status_t& status, uint8_t arg, uint8_t* res) {
        zb_ret_t ret = zb_set_tx_power(arg);
        if (ret != RET_OK) {
            status = GENERIC_INVALID_PARAMETER_1;
            *res = 0;
            ESP_LOGE(TAG, "Failed to set TX_Power (error: %d)", ERROR_GET_CODE(ret));
            return;
        }
        if (storage::app_config().tx_power != arg) {
            esp_err_t ret = storage::write_param(offsetof(storage::zb_app_config_t, tx_power), &arg, sizeof(arg));
            if (ret == ESP_OK) {
                storage::app_config().tx_power = arg;
            } else {
                ESP_LOGE(TAG, "Failed to write SET_TX_POWER to LittleFS Partition (error: %s)", esp_err_to_name(ret));
            }
        }
        *res = arg;
    }
};

template <> struct zb_ncp::cmd_handle<GET_TX_POWER> :
    immediate_cmd_process<GET_TX_POWER>,
    general_status_res<GET_TX_POWER, uint8_t> {
    static void process_status_res(ncp_generic_status_t& status, uint8_t* res) {
        *res = storage::app_config().tx_power;
    }
};

static_assert(sizeof(zb_zdo_active_ep_req_t) == 2);
#ifndef __INTELLISENSE__
static_assert(sizeof(zb_zdo_ep_resp_t) == 5);
#endif
template <> struct zb_ncp::cmd_handle<ZDO_ACTIVE_EP_REQ> :
    request_cmd_process<ZDO_ACTIVE_EP_REQ, uint16_t, zb_zdo_active_ep_req_t, zb_zdo_ep_resp_t> {
    static constexpr const char* name = "ZDO_ACTIVE_EP_REQ";
    static void format_request(zb_zdo_active_ep_req_t& req, const uint16_t& arg) {
        req.nwk_addr = arg;
    }
    static uint8_t start_request(uint8_t buf) {
        return zb_zdo_active_ep_req(buf, Cmd::req_callback);
    }
    static uint16_t get_outdata_len(const zb_zdo_ep_resp_t* resp, uint16_t buf_len) {
        return sizeof(resp->ep_count) + resp->ep_count + sizeof(resp->nwk_addr);
    }
    static ncp_generic_status_t get_response(const zb_zdo_ep_resp_t* resp, uint8_t* outdata, uint16_t outlen) {
        uint8_t* out_ptr = outdata;
        *out_ptr++ = resp->ep_count;
        if (resp->ep_count > 0) {
            memcpy(out_ptr, resp + 1, resp->ep_count);
            out_ptr += resp->ep_count;
        }
        ZB_LETOH16(out_ptr, &resp->nwk_addr);
        ESP_LOGI(TAG, "Registered Endpoints for NWK: %04x, Count: %d", resp->nwk_addr, resp->ep_count);
        return GENERIC_OK;
    }
};

struct ZDO_SIMPLE_DESC_REQ_resp_t {
    uint8_t endpoint;
    uint16_t profileID;
    uint16_t deviceID;
    uint8_t version;
    uint8_t inputClusterCount;
    uint8_t outputClusterCount;
} __attribute__((packed)) __attribute__((aligned(1)));
#ifndef __INTELLISENSE__
static_assert(sizeof(zb_zdo_simple_desc_req_t) == 3);
static_assert(sizeof(zb_zdo_simple_desc_resp_t) == 17);
#endif
template <> struct zb_ncp::cmd_handle<ZDO_SIMPLE_DESC_REQ> :
    request_cmd_process<ZDO_SIMPLE_DESC_REQ, zb_zdo_simple_desc_req_t, zb_zdo_simple_desc_req_t, zb_zdo_simple_desc_resp_t> {
    static constexpr const char* name = "ZDO_SIMPLE_DESC_REQ";
    static uint8_t start_request(uint8_t buf) {
        return zb_zdo_simple_desc_req(buf, Cmd::req_callback);
    }
    static uint16_t get_outdata_len(const zb_zdo_simple_desc_resp_t* resp, uint16_t buf_len) {
        return sizeof(ZDO_SIMPLE_DESC_REQ_resp_t) + (sizeof(uint16_t) * (resp->simple_desc.app_input_cluster_count + resp->simple_desc.app_output_cluster_count)) + sizeof(resp->hdr.nwk_addr);
    }
    static ncp_generic_status_t get_response(const zb_zdo_simple_desc_resp_t* resp, uint8_t* outdata, uint16_t outlen) {
        auto out_hdr = reinterpret_cast<ZDO_SIMPLE_DESC_REQ_resp_t*>(outdata);
        out_hdr->endpoint = resp->simple_desc.endpoint;
        out_hdr->profileID = resp->simple_desc.app_profile_id;
        out_hdr->deviceID = resp->simple_desc.app_device_id;
        out_hdr->version = resp->simple_desc.app_device_version;
        out_hdr->inputClusterCount = resp->simple_desc.app_input_cluster_count;
        out_hdr->outputClusterCount = resp->simple_desc.app_output_cluster_count;
        auto clusters_ptr = reinterpret_cast<uint16_t*>(out_hdr + 1);
        char in_clusts[out_hdr->inputClusterCount * 8 + 1];
        char* in_ch = in_clusts;
        *in_ch = '\0';
        for (uint8_t i = 0; i < out_hdr->inputClusterCount; ++i) {
            uint16_t cluster = *(resp->simple_desc.app_cluster_list + i);
            *clusters_ptr++ = cluster;
            in_ch += sprintf(in_ch, "0x%04x%s", cluster, (i == out_hdr->inputClusterCount - 1) ? "" : ", ");
        }
        char out_clusts[out_hdr->outputClusterCount * 8 + 1];
        char* out_ch = out_clusts;
        *out_ch = '\0';
        for (uint8_t i = 0; i < out_hdr->outputClusterCount; ++i) {
            uint16_t cluster = *(resp->simple_desc.app_cluster_list + out_hdr->inputClusterCount + i);
            *clusters_ptr++ = cluster;
            out_ch += sprintf(out_ch, "0x%04x%s", cluster, (i == out_hdr->outputClusterCount - 1) ? "" : ", ");
        }
        memcpy(clusters_ptr, &resp->hdr.nwk_addr, sizeof(resp->hdr.nwk_addr));
        ESP_LOGD(TAG, "Endpoint: %d, ProfileID: %04x, DeviceID: %04x, Version: %d, inputClusterCount: %d, outputClusterCount: %d",
            out_hdr->endpoint, out_hdr->profileID, out_hdr->deviceID, out_hdr->version, out_hdr->inputClusterCount, out_hdr->outputClusterCount);
        ESP_LOGD(TAG, "InputClusters: %s", in_clusts);
        ESP_LOGD(TAG, "OutputClusters: %s", out_clusts);
        return GENERIC_OK;
    }
};

struct GET_MODULE_VERSION_resp_t {
    uint32_t fwVersion;
    uint32_t stackVersion;
    uint32_t protocolVersion;
} __attribute__((packed)) __attribute__((aligned(1)));
template <> struct zb_ncp::cmd_handle<GET_MODULE_VERSION> :
    immediate_cmd_process<GET_MODULE_VERSION>,
    general_status_res<GET_MODULE_VERSION, GET_MODULE_VERSION_resp_t> {
    static void process_status_res(ncp_generic_status_t& status, GET_MODULE_VERSION_resp_t* res) {
        res->fwVersion = OTA_APP_VERSION;
        res->stackVersion = zboss_version_get();
        res->protocolVersion = ZB_PROTOCOL_VERSION;
    }
};

template <> struct zb_ncp::cmd_handle<GET_COORDINATOR_VERSION> :
    immediate_cmd_process<GET_COORDINATOR_VERSION>,
    general_status_res<GET_COORDINATOR_VERSION, uint8_t> {
    static void process_status_res(ncp_generic_status_t& status, uint8_t* res) {
        *res = zb_ncp::DEVICE_VERSION; // zb_aib_get_coordinator_version();
    }
};

template <> struct zb_ncp::cmd_handle<NCP_RESET> :
    immediate_cmd_process<NCP_RESET>,
    general_status_arg<NCP_RESET, uint8_t> {
    static void process_status_arg(ncp_generic_status_t& status, uint8_t arg) {
        bool should_restart = false;
        switch (arg) {
        case 0: // NoOptions
            should_restart = true;
            break;
        case 1: // EraseNVRAM
            ESP_LOGI(TAG, "Erase NVRAM");
            should_restart = true;
            zb_nvram_erase();
            storage::delete_config();
            break;
        case 2: // FactoryReset
            ESP_LOGI(TAG, "Factory Reset");
            should_restart = true;
            zb_bdb_reset_via_local_action(0);
            zb_nvram_erase();
            storage::delete_config();
            // storage::format();
            break;
        case 3: // LockReadingKeys
            ESP_LOGI(TAG, "Disable Unsecure Trust Center Rejoin");
            zb_secur_set_tc_rejoin_enabled(ZB_FALSE);
            zb_secur_set_ignore_tc_rejoin(ZB_TRUE);
            break;
        default:
            status = GENERIC_INVALID_PARAMETER_1;
            break;
        }
        if (should_restart) {
            app::ctx_t ncp_event = {
                .event = app::EVENT_RESET,
                .size = 100, // Pause 100 ms before Restart
                .buf_ptr = nullptr
            };
            app::send_event(ncp_event);
        }
    }
};

template <> struct zb_ncp::cmd_handle<SET_MAX_CHILDREN> :
    immediate_cmd_process<SET_MAX_CHILDREN>,
    general_status_arg<SET_MAX_CHILDREN, uint8_t> {
    static void process_status_arg(ncp_generic_status_t& status, uint8_t arg) {
        zb_set_max_children(arg);
        if (storage::app_config().max_children != arg) {
            esp_err_t ret = storage::write_param(offsetof(storage::zb_app_config_t, max_children), &arg, sizeof(arg));
            if (ret == ESP_OK) {
                storage::app_config().max_children = arg;
            } else {
                ESP_LOGE(TAG, "Failed to write SET_MAX_CHILDREN to LittleFS Partition (error: %s)", esp_err_to_name(ret));
            }
        }
    }
};

template <> struct zb_ncp::cmd_handle<GET_MAX_CHILDREN> :
    immediate_cmd_process<GET_MAX_CHILDREN>,
    general_status_res<GET_MAX_CHILDREN, uint8_t> {
    static void process_status_res(ncp_generic_status_t& status, uint8_t* res) {
        *res = zb_get_max_children();
    }
};

struct ZDO_PERMIT_JOINING_REQ_arg_t {
    uint16_t nwk;  // add by FIX
    uint8_t duration;
    uint8_t tcSignificance;
} __attribute__((packed)) __attribute__((aligned(1)));
static_assert(sizeof(zb_zdo_mgmt_permit_joining_req_param_t) == 4);
static_assert(sizeof(zb_zdo_mgmt_permit_joining_resp_t) == 2);
template <> struct zb_ncp::cmd_handle<ZDO_PERMIT_JOINING_REQ> :
    request_cmd_process<ZDO_PERMIT_JOINING_REQ, ZDO_PERMIT_JOINING_REQ_arg_t, zb_zdo_mgmt_permit_joining_req_param_t, zb_zdo_mgmt_permit_joining_resp_t> {
    static constexpr const alloc_t alloc_request = alloc_t::PARAM;
    static constexpr const char* name = "ZDO_PERMIT_JOINING_REQ";
    static uint16_t get_outdata_len(const zb_zdo_mgmt_permit_joining_resp_t* resp, uint16_t buf_len) {
        return 0;
    }
    static ncp_generic_status_t get_response(const zb_zdo_mgmt_permit_joining_resp_t* resp, uint8_t* outdata, uint16_t outlen) {
        return GENERIC_OK;
    }
    static uint8_t start_request(uint8_t buf) {
        return zb_zdo_mgmt_permit_joining_req(buf, Cmd::req_callback);
    }
    static void format_request(zb_zdo_mgmt_permit_joining_req_param_t& req, const ZDO_PERMIT_JOINING_REQ_arg_t& arg) {
        req.dest_addr = arg.nwk;
        req.permit_duration = arg.duration;
        req.tc_significance = arg.tcSignificance;
        ESP_LOGD(TAG, "%s::format_request NWK: %04x, Significance: %02x, Duration: %d s", Cmd::name, req.dest_addr, req.tc_significance, req.permit_duration);
    }
    static void buf_alloc(uint8_t req_idx) {
        auto& req = ResolveStrategy::get_by_index(req_idx);
        if (req.arg.nwk == 0xfffc) {
            ESP_LOGD(TAG, "%s::format_request NWK: %04x, Significance: %02x, Duration: %d s", Cmd::name, req.arg.nwk, req.arg.tcSignificance, req.arg.duration);
            if (req.arg.duration == 0) {
                zb_zgps_stop_commissioning();
            } else {
                zb_zgps_start_commissioning(req.arg.duration * ZB_TIME_ONE_SECOND);
            }
            ESP_LOGD(TAG, "%s::req_callback req_idx: %d", Cmd::name, req_idx);
            Cmd::send_response(req, nullptr, req.alloc_len);
            req.state.store(ResolveStrategy::state_t::S_NONE, std::memory_order_release);
            return;
        }
        auto ret = zb_buf_get_out_delayed_ext(Cmd::do_request, req_idx, req.alloc_len);
        if (ret != RET_OK) {
            report_failed(req.cmd, ZB_ZDP_STATUS_INSUFFICIENT_SPACE);
            req.state.store(ResolveStrategy::state_t::S_NONE, std::memory_order_release);
            return;
        }
    }
};

struct ZDO_NODE_DESC_resp_t {
    uint16_t flags;
    uint8_t macCapabilities;
    uint16_t manufacturerCode;
    uint8_t bufferSize;
    uint16_t incomingSize;
    uint16_t serverMask;
    uint16_t outgoingSize;
    uint8_t descriptorCapabilities;
    uint16_t nwk;
} __attribute__((packed)) __attribute__((aligned(1)));
static_assert(sizeof(zb_zdo_node_desc_req_t) == 2);
#ifndef __INTELLISENSE__
static_assert(sizeof(zb_zdo_node_desc_resp_t) == 17);
#endif
template <> struct zb_ncp::cmd_handle<ZDO_NODE_DESC_REQ> :
    request_cmd_process<ZDO_NODE_DESC_REQ, zb_zdo_node_desc_req_t, zb_zdo_node_desc_req_t, zb_zdo_node_desc_resp_t> {
    static constexpr const char* name = "ZDO_NODE_DESC_REQ";
    static uint8_t start_request(uint8_t buf) {
        return zb_zdo_node_desc_req(buf, Cmd::req_callback);
    }
    static uint16_t get_outdata_len(const zb_zdo_node_desc_resp_t* resp, uint16_t buf_len) {
        return sizeof(ZDO_NODE_DESC_resp_t);
    }
    static ncp_generic_status_t get_response(const zb_zdo_node_desc_resp_t* resp, uint8_t* outdata, uint16_t outlen) {
        auto out_hdr = reinterpret_cast<ZDO_NODE_DESC_resp_t*>(outdata);
        out_hdr->flags = resp->node_desc.node_desc_flags;
        out_hdr->macCapabilities = resp->node_desc.mac_capability_flags;
        out_hdr->manufacturerCode = resp->node_desc.manufacturer_code;
        out_hdr->bufferSize = resp->node_desc.max_buf_size;
        out_hdr->incomingSize = resp->node_desc.max_incoming_transfer_size;
        out_hdr->serverMask = resp->node_desc.server_mask;
        out_hdr->outgoingSize = resp->node_desc.max_outgoing_transfer_size;
        out_hdr->descriptorCapabilities = resp->node_desc.desc_capability_field;
        out_hdr->nwk = resp->hdr.nwk_addr;
        return GENERIC_OK;
    }
};

struct ZDO_MGMT_LQI_REQ_arg_t {
    uint16_t nwk;  // add by FIX
    uint8_t startIndex;
} __attribute__((packed)) __attribute__((aligned(1)));
struct ZDO_MGMT_LQI_REQ_resp_t {
    uint8_t total_entries;
    uint8_t startIndex;
    uint8_t list_count;
} __attribute__((packed)) __attribute__((aligned(1)));
static_assert(sizeof(zb_zdo_mgmt_lqi_param_t) == 4);
static_assert(sizeof(zb_zdo_mgmt_lqi_resp_t) == 5);
static_assert(sizeof(zb_zdo_neighbor_table_record_t) == 22);
template <> struct zb_ncp::cmd_handle<ZDO_MGMT_LQI_REQ> :
    request_cmd_process<ZDO_MGMT_LQI_REQ, ZDO_MGMT_LQI_REQ_arg_t, zb_zdo_mgmt_lqi_param_t, zb_zdo_mgmt_lqi_resp_t> {
    static constexpr const alloc_t alloc_request = alloc_t::PARAM;
    static constexpr const char* name = "ZDO_MGMT_LQI_REQ";
    static uint16_t get_outdata_len(const zb_zdo_mgmt_lqi_resp_t* resp, uint16_t buf_len) {
        return sizeof(ZDO_MGMT_LQI_REQ_resp_t) + (resp->neighbor_table_list_count * sizeof(zb_zdo_neighbor_table_record_t));
    }
    static ncp_generic_status_t get_response(const zb_zdo_mgmt_lqi_resp_t* resp, uint8_t* outdata, uint16_t outlen) {
        auto hdr = reinterpret_cast<ZDO_MGMT_LQI_REQ_resp_t*>(outdata);
        hdr->total_entries = resp->neighbor_table_entries;
        hdr->startIndex = resp->start_index;
        hdr->list_count = resp->neighbor_table_list_count;
        if (hdr->list_count > 0) {
            memcpy(hdr + 1, resp + 1, hdr->list_count * sizeof(zb_zdo_neighbor_table_record_t));
        }
        return GENERIC_OK;
    }
    static uint8_t start_request(uint8_t buf) {
        return zb_zdo_mgmt_lqi_req(buf, Cmd::req_callback);
    }
    static void format_request(zb_zdo_mgmt_lqi_param_t& req, const ZDO_MGMT_LQI_REQ_arg_t& arg) {
        req.dst_addr = arg.nwk;
        req.start_index = arg.startIndex;
    }
};

struct ZDO_MGMT_LEAVE_REQ_arg_t {
    uint16_t nwk;  // add by FIX
    zb_ieee_addr_t ieee;
    uint8_t flags;
} __attribute__((packed)) __attribute__((aligned(1)));
#ifndef __INTELLISENSE__
static_assert(sizeof(zb_zdo_mgmt_leave_param_t) == 11);
#endif
static_assert(sizeof(zb_zdo_mgmt_leave_res_t) == 2);
template <> struct zb_ncp::cmd_handle<ZDO_MGMT_LEAVE_REQ> : 
    request_cmd_process<ZDO_MGMT_LEAVE_REQ, ZDO_MGMT_LEAVE_REQ_arg_t, zb_zdo_mgmt_leave_param_t, zb_zdo_mgmt_leave_res_t> {
    static constexpr const alloc_t alloc_request = alloc_t::PARAM;
    static constexpr const char* name = "ZDO_MGMT_LEAVE_REQ";
    static uint16_t get_outdata_len(const zb_zdo_mgmt_leave_res_t* resp, uint16_t buf_len) {
        return 0;
    }
    static ncp_generic_status_t get_response(const zb_zdo_mgmt_leave_res_t* resp, uint8_t* outdata, uint16_t outlen) {
        return GENERIC_OK;
    }
    static uint8_t start_request(uint8_t buf) {
        return zdo_mgmt_leave_req(buf, Cmd::req_callback);
    }
    static void format_request(zb_zdo_mgmt_leave_param_t& req, const ZDO_MGMT_LEAVE_REQ_arg_t& arg) {
        memcpy(req.device_address, arg.ieee, sizeof(arg.ieee));
        req.dst_addr = arg.nwk;
        req.remove_children = ZB_TRUE; // (arg.flags & 0x40) ? ZB_TRUE : ZB_FALSE;
        req.rejoin = (arg.flags & 0x80) ? ZB_TRUE : ZB_FALSE;
        ESP_LOGD(TAG, "%s::format_request NWK: %04x, flags: %d, remove_children: %d, rejoin: %d", Cmd::name, req.dst_addr, arg.flags, req.remove_children, req.rejoin);
    }
};

struct ZDO_IEEE_ADDR_REQ_arg_t {
    uint16_t destNwk; // add by FIX
    uint16_t nwk;
    uint8_t type;
    uint8_t startIndex;
} __attribute__((packed)) __attribute__((aligned(1)));
struct ZDO_IEEE_ADDR_REQ_resp_t {
    zb_ieee_addr_t ieee;
    uint16_t nwk;
} __attribute__((packed)) __attribute__((aligned(1)));
static_assert(sizeof(zb_zdo_ieee_addr_resp_t) == 12);
static_assert(sizeof(zb_zdo_ieee_addr_req_param_t) == 6);
template <> struct zb_ncp::cmd_handle<ZDO_IEEE_ADDR_REQ> :
    request_cmd_process<ZDO_IEEE_ADDR_REQ, ZDO_IEEE_ADDR_REQ_arg_t, zb_zdo_ieee_addr_req_param_t, zb_zdo_ieee_addr_resp_t> {
    static constexpr const alloc_t alloc_request = alloc_t::PARAM;
    static constexpr const char* name = "ZDO_IEEE_ADDR_REQ";
    static uint16_t get_outdata_len(const zb_zdo_ieee_addr_resp_t* resp, uint16_t buf_len) {
        return buf_len - sizeof(resp->tsn) - sizeof(resp->status);
    }
    static ncp_generic_status_t get_response(const zb_zdo_ieee_addr_resp_t* resp, uint8_t* outdata, uint16_t outlen) {
        auto hdr = reinterpret_cast<ZDO_IEEE_ADDR_REQ_resp_t*>(outdata);
        ZB_LETOH64(hdr->ieee, resp->ieee_addr_remote_dev);
        ZB_LETOH16(&hdr->nwk, &resp->nwk_addr_remote_dev);
        if (outlen > sizeof(ZDO_IEEE_ADDR_REQ_resp_t)) {  // if (req.arg.request_type == 0x01)
            auto ext = reinterpret_cast<const zb_zdo_ieee_addr_resp_ext_t*>(resp + 1);
            auto out = reinterpret_cast<uint8_t*>(hdr + 1);
            uint8_t num = ext->num_assoc_dev;
            *out++ = num;
            if (outlen > (sizeof(ZDO_IEEE_ADDR_REQ_resp_t) + sizeof(zb_zdo_ieee_addr_resp_ext_t))) {
                auto ext2 = reinterpret_cast<const zb_zdo_ieee_addr_resp_ext2_t*>(ext + 1);
                *out++ = ext2->start_index;
                auto nwks = reinterpret_cast<const uint16_t*>(ext2 + 1);
                for (uint8_t i = 0; i < num; ++i) {
                    memcpy(out, nwks++, sizeof(uint16_t));
                    out += sizeof(uint16_t);
                }
            }
        }
        return GENERIC_OK;
    }
    static uint8_t start_request(uint8_t buf) {
        return zb_zdo_ieee_addr_req(buf, Cmd::req_callback);
    }
    static void format_request(zb_zdo_ieee_addr_req_param_t& req, const ZDO_IEEE_ADDR_REQ_arg_t& arg) {
        req.dst_addr = arg.destNwk;
        req.nwk_addr = arg.nwk;
        req.request_type = arg.type;
        req.start_index = arg.startIndex;
        ESP_LOGD(TAG, "%s::format_request DestNWK: %04x, NWK: %04x, startIndex: %d, request_type: %d", Cmd::name, req.dst_addr, req.nwk_addr, req.start_index, req.request_type);
    }
};

struct ZDO_BIND_REQ_arg_t {
    uint16_t target; // add by FIX
    zb_ieee_addr_t srcIeee;
    uint8_t srcEP;
    uint16_t clusterID;
    uint8_t addrMode;
} __attribute__((packed)) __attribute__((aligned(1)));
static_assert(sizeof(zb_zdo_bind_resp_t) == 2);
#ifndef __INTELLISENSE__
static_assert(sizeof(zb_zdo_bind_req_param_t) == 24);
#endif
template <> struct zb_ncp::cmd_handle<ZDO_BIND_REQ> :
    request_cmd_process<ZDO_BIND_REQ, ZDO_BIND_REQ_arg_t, zb_zdo_bind_req_param_t, zb_zdo_bind_resp_t> {
    static constexpr const alloc_t alloc_request = alloc_t::PARAM;
    static constexpr const char* name = "ZDO_BIND_REQ";
    static uint16_t get_outdata_len(const zb_zdo_bind_resp_t* resp, uint16_t buf_len) {
        return 0;
    }
    static ncp_generic_status_t get_response(const zb_zdo_bind_resp_t* resp, uint8_t* outdata, uint16_t outlen) {
        return GENERIC_OK;
    }
    static uint8_t start_request(uint8_t buf) {
        return zb_zdo_bind_req(buf, Cmd::req_callback);
    }
    static bool check_arg_size(const void* buffer, uint16_t len) {
        if (len < sizeof(ZDO_BIND_REQ_arg_t)) return false;
        auto arg = static_cast<const ZDO_BIND_REQ_arg_t*>(buffer);
        if (arg->addrMode == ZB_BIND_DST_ADDR_MODE_64_BIT_EXTENDED) {
            return len == (sizeof(ZDO_BIND_REQ_arg_t) + sizeof(zb_ieee_addr_t) + sizeof(uint8_t));
        } else {
            return len == (sizeof(ZDO_BIND_REQ_arg_t) + sizeof(uint16_t));
        }
    }
    static void format_request(zb_zdo_bind_req_param_t& req, const ZDO_BIND_REQ_arg_t& arg) {
        req.req_dst_addr = arg.target;
        memcpy(req.src_address, arg.srcIeee, sizeof(arg.srcIeee));
        req.src_endp = arg.srcEP;
        req.cluster_id = arg.clusterID;
        req.dst_addr_mode = arg.addrMode;
        const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&arg + 1);
        if (arg.addrMode == ZB_BIND_DST_ADDR_MODE_64_BIT_EXTENDED) {
            memcpy(req.dst_address.addr_long, ptr, sizeof(req.dst_address.addr_long));
            ptr += sizeof(req.dst_address.addr_long);
            req.dst_endp = *ptr;
        } else {
            memcpy(&req.dst_address.addr_short, ptr, 2);
            req.dst_endp = 0;
        }
    }
};

template <> struct zb_ncp::cmd_handle<ZDO_UNBIND_REQ> :
    request_cmd_process<ZDO_UNBIND_REQ, ZDO_BIND_REQ_arg_t, zb_zdo_bind_req_param_t, zb_zdo_bind_resp_t> {
    static constexpr const alloc_t alloc_request = alloc_t::PARAM;
    static constexpr const char* name = "ZDO_UNBIND_REQ";
    static uint16_t get_outdata_len(const zb_zdo_bind_resp_t* resp, uint16_t buf_len) {
        return 0;
    }
    static ncp_generic_status_t get_response(const zb_zdo_bind_resp_t* resp, uint8_t* outdata, uint16_t outlen) {
        return GENERIC_OK;
    }
    static uint8_t start_request(uint8_t buf) {
        return zb_zdo_unbind_req(buf, Cmd::req_callback);
    }
    static bool check_arg_size(const void* buffer, uint16_t len) {
        if (len < sizeof(ZDO_BIND_REQ_arg_t)) return false;
        auto arg = static_cast<const ZDO_BIND_REQ_arg_t*>(buffer);
        if (arg->addrMode == ZB_BIND_DST_ADDR_MODE_64_BIT_EXTENDED) {
            return len == (sizeof(ZDO_BIND_REQ_arg_t) + sizeof(zb_ieee_addr_t) + sizeof(uint8_t));
        }
        else {
            return len == (sizeof(ZDO_BIND_REQ_arg_t) + sizeof(uint16_t));
        }
    }
    static void format_request(zb_zdo_bind_req_param_t& req, const ZDO_BIND_REQ_arg_t& arg) {
        req.req_dst_addr = arg.target;
        memcpy(req.src_address, arg.srcIeee, sizeof(arg.srcIeee));
        req.src_endp = arg.srcEP;
        req.cluster_id = arg.clusterID;
        req.dst_addr_mode = arg.addrMode;
        const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&arg + 1);
        if (arg.addrMode == ZB_BIND_DST_ADDR_MODE_64_BIT_EXTENDED) {
            memcpy(req.dst_address.addr_long, ptr, sizeof(req.dst_address.addr_long));
            ptr += sizeof(req.dst_address.addr_long);
            req.dst_endp = *ptr;
        }
        else {
            memcpy(&req.dst_address.addr_short, ptr, 2);
            req.dst_endp = 0;
        }
    }
};

struct ZDO_POWER_DESC_REQ_resp_t {
    uint16_t powerDescriptor;
    uint16_t nwk;
} __attribute__((packed)) __attribute__((aligned(1)));
static_assert(sizeof(zb_zdo_power_desc_req_t) == 2);
static_assert(sizeof(zb_zdo_power_desc_resp_t) == 6);
template <> struct zb_ncp::cmd_handle<ZDO_POWER_DESC_REQ> :
    request_cmd_process<ZDO_POWER_DESC_REQ, zb_zdo_power_desc_req_t, zb_zdo_power_desc_req_t, zb_zdo_power_desc_resp_t> {
    static constexpr const char* name = "ZDO_POWER_DESC_REQ";
    static uint8_t start_request(uint8_t buf) {
        return zb_zdo_power_desc_req(buf, Cmd::req_callback);
    }
    static uint16_t get_outdata_len(const zb_zdo_power_desc_resp_t* resp, uint16_t buf_len) {
        return sizeof(ZDO_POWER_DESC_REQ_resp_t);
    }
    static ncp_generic_status_t get_response(const zb_zdo_power_desc_resp_t* resp, uint8_t* outdata, uint16_t outlen) {
        auto out_hdr = reinterpret_cast<ZDO_POWER_DESC_REQ_resp_t*>(outdata);
        out_hdr->powerDescriptor = resp->power_desc.power_desc_flags;
        out_hdr->nwk = resp->hdr.nwk_addr;
        return GENERIC_OK;
    }
};

struct ZDO_NWK_ADDR_REQ_arg_t {
    uint16_t nwk; // add by FIX
    zb_ieee_addr_t ieee;
    uint8_t type;
    uint8_t startIndex;
} __attribute__((packed)) __attribute__((aligned(1)));
struct ZDO_NWK_ADDR_REQ_resp_t {
    zb_ieee_addr_t ieee;
    uint16_t nwk;
} __attribute__((packed)) __attribute__((aligned(1)));
static_assert(sizeof(zb_zdo_nwk_addr_req_param_t) == 12);
static_assert(sizeof(zb_zdo_nwk_addr_resp_head_t) == 12);
static_assert(sizeof(zb_zdo_nwk_addr_resp_ext_t) == 1);
static_assert(sizeof(zb_zdo_nwk_addr_resp_ext2_t) == 1);
template <> struct zb_ncp::cmd_handle<ZDO_NWK_ADDR_REQ> :
    request_cmd_process<ZDO_NWK_ADDR_REQ, ZDO_NWK_ADDR_REQ_arg_t, zb_zdo_nwk_addr_req_param_t, zb_zdo_nwk_addr_resp_head_t> {
    static constexpr const alloc_t alloc_request = alloc_t::TAIL;
    static constexpr const char* name = "ZDO_NWK_ADDR_REQ";
    static uint16_t get_outdata_len(const zb_zdo_nwk_addr_resp_head_t* resp, uint16_t buf_len) {
        return buf_len - sizeof(resp->tsn) - sizeof(resp->status);
    }
    static ncp_generic_status_t get_response(const zb_zdo_nwk_addr_resp_head_t* resp, uint8_t* outdata, uint16_t outlen) {
        auto hdr = reinterpret_cast<ZDO_NWK_ADDR_REQ_resp_t*>(outdata);
        ZB_LETOH64(hdr->ieee, resp->ieee_addr);
        ZB_LETOH16(&hdr->nwk, &resp->nwk_addr);
        if (outlen > sizeof(ZDO_NWK_ADDR_REQ_resp_t)) {
            auto ext = reinterpret_cast<const zb_zdo_nwk_addr_resp_ext_t*>(resp + 1);
            auto out = reinterpret_cast<uint8_t*>(hdr + 1);
            uint8_t num = ext->num_assoc_dev;
            *out++ = num;
            if (outlen > (sizeof(ZDO_NWK_ADDR_REQ_resp_t) + sizeof(zb_zdo_nwk_addr_resp_ext_t))) {
                auto ext2 = reinterpret_cast<const zb_zdo_nwk_addr_resp_ext2_t*>(ext + 1);
                *out++ = ext2->start_index;
                auto nwks = reinterpret_cast<const uint16_t*>(ext2 + 1);
                for (uint8_t i = 0; i < num; ++i) {
                    memcpy(out, nwks++, sizeof(uint16_t));
                    out += sizeof(uint16_t);
                }
            }
        }
        return GENERIC_OK;
    }
    static uint8_t start_request(uint8_t buf) {
        return zb_zdo_nwk_addr_req(buf, Cmd::req_callback);
    }
    static void format_request(zb_zdo_nwk_addr_req_param_t& req, const ZDO_NWK_ADDR_REQ_arg_t& arg) {
        req.dst_addr = arg.nwk;
        memcpy(req.ieee_addr, arg.ieee, sizeof(arg.ieee));
        req.request_type = arg.type;
        req.start_index = arg.startIndex;
        ESP_LOGD(TAG, "%s::format_request NWK: %04x, startIndex: %d, request_type: %d", Cmd::name, req.dst_addr, req.start_index, req.request_type);
    }
};

struct ZDO_MATCH_DESC_REQ_arg_t {
    uint16_t nwk;
    uint16_t profileID;
    uint8_t inputClusterCount;
} __attribute__((packed)) __attribute__((aligned(1)));
static_assert(sizeof(zb_zdo_match_desc_param_t) == 10);
#ifndef __INTELLISENSE__
static_assert(sizeof(zb_zdo_match_desc_resp_t) == 5);
#endif
template <> struct zb_ncp::cmd_handle<ZDO_MATCH_DESC_REQ> :
    request_cmd_process<ZDO_MATCH_DESC_REQ, ZDO_MATCH_DESC_REQ_arg_t, zb_zdo_match_desc_param_t, zb_zdo_match_desc_resp_t> {
    static constexpr const char* name = "ZDO_MATCH_DESC_REQ";
    static uint16_t get_outdata_len(const zb_zdo_match_desc_resp_t* resp, uint16_t buf_len) {
        return sizeof(resp->match_len) + resp->match_len + sizeof(resp->nwk_addr);
    }
    static ncp_generic_status_t get_response(const zb_zdo_match_desc_resp_t* resp, uint8_t* outdata, uint16_t outlen) {
        uint8_t* out_ptr = outdata;
        *out_ptr++ = resp->match_len;
        if (resp->match_len > 0) {
            memcpy(out_ptr, resp + 1, resp->match_len);
            out_ptr += resp->match_len;
        }
        ZB_LETOH16(out_ptr, &resp->nwk_addr);
        return GENERIC_OK;
    }
    static uint8_t start_request(uint8_t buf) {
        return zb_zdo_match_desc_req(buf, Cmd::req_callback);
    }
    static bool check_arg_size(const void* buffer, uint16_t len) {
        if (len <= sizeof(ZDO_MATCH_DESC_REQ_arg_t)) return false;
        auto arg = static_cast<const ZDO_MATCH_DESC_REQ_arg_t*>(buffer);
        auto out_count_pos = sizeof(ZDO_MATCH_DESC_REQ_arg_t) + (arg->inputClusterCount * sizeof(uint16_t));
        uint8_t outputClusterCount = *(static_cast<const uint8_t*>(buffer) + out_count_pos);
        return len == (sizeof(ZDO_MATCH_DESC_REQ_arg_t) + sizeof(outputClusterCount) + ((arg->inputClusterCount + outputClusterCount) * sizeof(uint16_t)));
    }
    static uint16_t get_request_alloc_size(const ZDO_MATCH_DESC_REQ_arg_t& arg) {
        auto out_count_pos = sizeof(ZDO_MATCH_DESC_REQ_arg_t) + (arg.inputClusterCount * sizeof(uint16_t));
        uint8_t outputClusterCount = *(reinterpret_cast<const uint8_t*>(&arg) + out_count_pos);
        return sizeof(zb_zdo_match_desc_param_t) - sizeof(uint16_t) + ((arg.inputClusterCount + outputClusterCount) * sizeof(uint16_t));
    }
    static void format_request(zb_zdo_match_desc_param_t& req, const ZDO_MATCH_DESC_REQ_arg_t& arg) {
        req.nwk_addr = arg.nwk;
        req.addr_of_interest = arg.nwk;
        req.profile_id = arg.profileID;
        req.num_in_clusters = arg.inputClusterCount;
        const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&arg + 1);
        auto in_clusters_size = arg.inputClusterCount * sizeof(uint16_t);
        if (arg.inputClusterCount > 0) {
            memcpy(req.cluster_list, ptr, in_clusters_size);
        }
        ptr += in_clusters_size;
        req.num_out_clusters = *ptr;
        if (req.num_out_clusters > 0) {
            memcpy(&req.cluster_list[arg.inputClusterCount], ptr + sizeof(req.num_out_clusters), req.num_out_clusters * sizeof(uint16_t));
        }
    }
};

struct ZDO_MGMT_BIND_REQ_arg_t {
    uint16_t nwk;  // add by FIX
    uint8_t startIndex;
} __attribute__((packed)) __attribute__((aligned(1)));
struct ZDO_MGMT_BIND_REQ_resp_t {
    uint8_t bindingTableEntries;
    uint8_t startIndex;
    uint8_t entryCount;
} __attribute__((packed)) __attribute__((aligned(1)));
static_assert(sizeof(zb_zdo_mgmt_bind_param_t) == 4);
static_assert(sizeof(zb_zdo_mgmt_bind_resp_t) == 5);
#ifndef __INTELLISENSE__
static_assert(sizeof(zb_zdo_binding_table_record_t) == 21);
#endif
template <> struct zb_ncp::cmd_handle<ZDO_MGMT_BIND_REQ> :
    request_cmd_process<ZDO_MGMT_BIND_REQ, ZDO_MGMT_BIND_REQ_arg_t, zb_zdo_mgmt_bind_param_t, zb_zdo_mgmt_bind_resp_t> {
    static constexpr const alloc_t alloc_request = alloc_t::PARAM;
    static constexpr const char* name = "ZDO_MGMT_BIND_REQ";
    static uint16_t get_outdata_len(const zb_zdo_mgmt_bind_resp_t* resp, uint16_t buf_len) {
        uint16_t outlen = buf_len - sizeof(resp->tsn) - sizeof(resp->status);
        if (outlen <= sizeof(ZDO_MGMT_BIND_REQ_resp_t)) return sizeof(ZDO_MGMT_BIND_REQ_resp_t);
        outlen = sizeof(ZDO_MGMT_BIND_REQ_resp_t);
        if (resp->binding_table_list_count > 0) {
            auto src = reinterpret_cast<const zb_zdo_binding_table_record_t*>(resp + 1);
            outlen += (resp->binding_table_list_count * (sizeof(src->src_address) + sizeof(src->src_endp) + sizeof(src->cluster_id) + sizeof(src->dst_addr_mode)));
            for (uint8_t i = 0; i < resp->binding_table_list_count; ++i) {
                if (src->dst_addr_mode == ZB_BIND_DST_ADDR_MODE_64_BIT_EXTENDED) {
                    outlen += sizeof(src->dst_address.addr_long);
                    outlen += sizeof(src->dst_endp);
                } else {
                    outlen += sizeof(src->dst_address.addr_short);
                }
                ++src;
            }
        }
        return outlen;
    }
    static ncp_generic_status_t get_response(const zb_zdo_mgmt_bind_resp_t* resp, uint8_t* outdata, uint16_t outlen) {
        auto hdr = reinterpret_cast<ZDO_MGMT_BIND_REQ_resp_t*>(outdata);
        hdr->bindingTableEntries = resp->binding_table_entries;
        hdr->startIndex = resp->start_index;
        hdr->entryCount = resp->binding_table_list_count;
        if (outlen > sizeof(ZDO_MGMT_BIND_REQ_resp_t)) {
            auto out = reinterpret_cast<uint8_t*>(hdr + 1);
            auto src = reinterpret_cast<const zb_zdo_binding_table_record_t*>(resp + 1);
            for (uint8_t i = 0; i < resp->binding_table_list_count; ++i) {
                memcpy(out, src->src_address, sizeof(src->src_address));
                out += sizeof(src->src_address);
                *out++ = src->src_endp;
                memcpy(out, &src->cluster_id, sizeof(src->cluster_id));
                out += sizeof(src->cluster_id);
                *out++ = src->dst_addr_mode;
                if (src->dst_addr_mode == ZB_BIND_DST_ADDR_MODE_64_BIT_EXTENDED) {
                    memcpy(out, src->dst_address.addr_long, sizeof(src->dst_address.addr_long));
                    out += sizeof(src->dst_address.addr_long);
                    *out++ = src->dst_endp;
                } else {
                    memcpy(out, &src->dst_address.addr_short, sizeof(src->dst_address.addr_short));
                    out += sizeof(src->dst_address.addr_short);
                } 
                ++src;
            }
        } else {
            hdr->entryCount = 0;
        }
        return GENERIC_OK;
    }
    static uint8_t start_request(uint8_t buf) {
        return zb_zdo_mgmt_bind_req(buf, Cmd::req_callback);
    }
    static void format_request(zb_zdo_mgmt_bind_param_t& req, const ZDO_MGMT_BIND_REQ_arg_t& arg) {
        req.dst_addr = arg.nwk;
        req.start_index = arg.startIndex;
    }
};

struct ZDO_MGMT_NWK_UPDATE_REQ_arg_t {
    uint16_t nwk;
    uint32_t channels;
    uint8_t duration;
} __attribute__((packed)) __attribute__((aligned(1)));
struct ZDO_MGMT_NWK_UPDATE_REQ_resp_t {
    uint32_t scannedChannels;
    uint16_t totalTransmissions;
    uint16_t totalFailures;
    uint8_t entryCount;
} __attribute__((packed)) __attribute__((aligned(1)));
#ifndef __INTELLISENSE__
static_assert(sizeof(zb_zdo_mgmt_nwk_update_req_t) == 10);
static_assert(sizeof(zb_zdo_mgmt_nwk_update_notify_hdr_t) == 11);
#endif
template <> struct zb_ncp::cmd_handle<ZDO_MGMT_NWK_UPDATE_REQ> :
    request_cmd_process<ZDO_MGMT_NWK_UPDATE_REQ, ZDO_MGMT_NWK_UPDATE_REQ_arg_t, zb_zdo_mgmt_nwk_update_req_t, zb_zdo_mgmt_nwk_update_notify_hdr_t> {
    static constexpr const alloc_t alloc_request = alloc_t::PARAM;
    static constexpr const char* name = "ZDO_MGMT_NWK_UPDATE_REQ";
    static uint16_t get_outdata_len(const zb_zdo_mgmt_nwk_update_notify_hdr_t* resp, uint16_t buf_len) {
        return sizeof(ZDO_MGMT_NWK_UPDATE_REQ_resp_t) + resp->scanned_channels_list_count;
    }
    static ncp_generic_status_t get_response(const zb_zdo_mgmt_nwk_update_notify_hdr_t* resp, uint8_t* outdata, uint16_t outlen) {
        auto hdr = reinterpret_cast<ZDO_MGMT_NWK_UPDATE_REQ_resp_t*>(outdata);
        hdr->scannedChannels = resp->scanned_channels;
        hdr->totalTransmissions = resp->total_transmissions;
        hdr->totalFailures = resp->transmission_failures;
        hdr->entryCount = resp->scanned_channels_list_count;
        if (hdr->entryCount > 0) {
            // auto list = reinterpret_cast<const zb_zdo_mgmt_nwk_update_notify_param_t*>(resp);
            const uint8_t* energy_values = reinterpret_cast<const uint8_t*>(resp + 1);
            memcpy(hdr + 1, energy_values, resp->scanned_channels_list_count); // list->energy_values
        }
        return GENERIC_OK;
    }
    static uint8_t start_request(uint8_t buf) {
        return zb_zdo_mgmt_nwk_update_req(buf, Cmd::req_callback);
    }
    static void format_request(zb_zdo_mgmt_nwk_update_req_t& req, const ZDO_MGMT_NWK_UPDATE_REQ_arg_t& arg) {
        req.dst_addr = arg.nwk;
        req.hdr.scan_channels = arg.channels;
        req.hdr.scan_duration = arg.duration;
        req.scan_count = 0;
        req.manager_addr = 0;
        const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&arg + 1);
        if (arg.duration <= 0x05) {
            req.scan_count = *ptr++;
        } else if (arg.duration == 0xfe || arg.duration == 0xff) {
            ptr++;
        }
        if (arg.duration == 0xff) {
            memcpy(&req.manager_addr, ptr, sizeof(req.manager_addr));
        }
    }
    static bool check_arg_size(const void* buffer, uint16_t len) {
        if (len < sizeof(ZDO_MGMT_NWK_UPDATE_REQ_arg_t)) return false;
        auto arg = static_cast<const ZDO_MGMT_NWK_UPDATE_REQ_arg_t*>(buffer);
        uint8_t expected_len = sizeof(ZDO_MGMT_NWK_UPDATE_REQ_arg_t);
        if (arg->duration <= 0x05) {
            expected_len += sizeof(uint8_t);
        } else if (arg->duration == 0xfe || arg->duration == 0xff) {
            expected_len += sizeof(uint8_t);
        }
        if (arg->duration == 0xff) {
            expected_len += sizeof(uint16_t);
        }
        return len == expected_len;
    }
};

struct zb_apsde_data_req_t {
    uint8_t paramLength;
    uint16_t dataLength;
    zb_addr_u addr;
    uint16_t profileID;
    uint16_t clusterID;
    uint8_t dstEndpoint; // exists if dstAddrMode = 2 or 3
    uint8_t srcEndpoint;
    uint8_t radius;
    uint8_t dstAddrMode;
    uint8_t txOptions;
    uint8_t useAlias;
    uint16_t aliasAddr;
    uint8_t aliasSequence;
} __attribute__((packed)) __attribute__((aligned(1)));
struct zb_apsde_data_req_nop_t {
    uint8_t paramLength;
    uint16_t dataLength;
    zb_addr_u addr;
    uint16_t profileID;
    uint16_t clusterID;
    uint8_t srcEndpoint;
    uint8_t radius;
    uint8_t dstAddrMode;
    uint8_t txOptions;
    uint8_t useAlias;
    uint16_t aliasAddr;
    uint8_t aliasSequence;
} __attribute__((packed)) __attribute__((aligned(1)));
struct APSDE_DATA_REQ_arg_t {
    union {
        zb_apsde_data_req_t hdr;
        zb_apsde_data_req_nop_t hdr_nop;
    };
    uint8_t data[OTA_MAX_DATA_SIZE];
} __attribute__((packed)) __attribute__((aligned(1)));
struct zb_apsde_data_confirm_t {
    zb_addr_u dst_addr;
    uint8_t dst_endpoint;
    uint8_t src_endpoint;
    zb_time_t tx_time;
    uint8_t dst_addr_mode;
    zb_ret_t status;
    zb_bool_t need_unlock;
    uint8_t _;
} __attribute__((packed)) __attribute__((aligned(1)));
struct zb_apsde_data_confirm_hdr_t {
    // uint8_t fc;
    // uint8_t dst_endpoint;
    uint16_t cluster_id;
    uint16_t profile_id;
    uint8_t src_endpoint;
    uint8_t tsn;
} __attribute__((packed)) __attribute__((aligned(1)));
static_assert(sizeof(zb_apsde_data_confirm_hdr_t) == 6);
static_assert(sizeof(zb_apsde_data_confirm_t) == 25);
template <> struct zb_ncp::cmd_handle<APSDE_DATA_REQ> :
    request_cmd_process<APSDE_DATA_REQ, APSDE_DATA_REQ_arg_t, APSDE_DATA_REQ_arg_t, zb_apsde_data_confirm_t> {
    static constexpr const char* name = "APSDE_DATA_REQ";
    static uint16_t get_outdata_len(const zb_apsde_data_confirm_t* resp, uint16_t buf_len) {
        return sizeof(resp->dst_addr.addr_long) + ((resp->dst_addr_mode == 2 || resp->dst_addr_mode == 3) ? sizeof(resp->dst_endpoint) : 0) +
            sizeof(resp->src_endpoint) + sizeof(uint32_t) + sizeof(resp->dst_addr_mode);
    }
    static ncp_generic_status_t get_response(const zb_apsde_data_confirm_t* resp, uint8_t* outdata, uint16_t outlen) {
        uint8_t* out_ptr = outdata;
        memcpy(out_ptr, &resp->dst_addr.addr_long, sizeof(resp->dst_addr.addr_long));
        out_ptr += sizeof(resp->dst_addr.addr_long);
        if (resp->dst_addr_mode == 2 || resp->dst_addr_mode == 3) {
            *out_ptr++ = resp->dst_endpoint;
        }
        *out_ptr++ = resp->src_endpoint;
        *reinterpret_cast<uint32_t*>(out_ptr) = (uint32_t)resp->tx_time;
        out_ptr += sizeof(uint32_t);
        *out_ptr = resp->dst_addr_mode;
        return GENERIC_OK;
    }
    static void req_callback(uint8_t buf) {
        if (buf) {
            auto status = zb_buf_get_status(buf);
            uint16_t buf_len = zb_buf_len(buf);
            auto resp = ZB_BUF_GET_PARAM(buf, const zb_apsde_data_confirm_t);
            auto buf_ptr = static_cast<const uint8_t*>(zb_buf_begin(buf));
            zb_uint8_t exp_len;
            zb_aps_get_aps_payload(buf, &exp_len);
            uint8_t fc = *buf_ptr;
            // ESP_LOGW(TAG, "---> FC: 0x%02x", fc);
            uint8_t frame_type = ZB_APS_FC_GET_FRAME_TYPE(fc);
            uint16_t len = buf_len;
            const uint8_t* data_ptr = buf_ptr;
            if (frame_type == ZB_APS_FRAME_DATA) {
                uint8_t delivery_mode = ZB_APS_FC_GET_DELIVERY_MODE(fc);
                if (delivery_mode == ZB_APS_DELIVERY_GROUP) {
                    len = sizeof(fc) + sizeof(uint16_t);
                } else {
                    len = sizeof(fc) + sizeof(uint8_t);
                }
                auto hdr = reinterpret_cast<const zb_apsde_data_confirm_hdr_t*>(buf_ptr + len);
                len += sizeof(zb_apsde_data_confirm_hdr_t);
                data_ptr = reinterpret_cast<const uint8_t*>(hdr + 1);
                len = (buf_len > len) ? buf_len - len : buf_len;
                if (unlikely(len != exp_len)) {
                    ESP_LOGE(TAG, "Buffers mismatch in aps_user_payload_callback(). FC: 0x%02x, len: %d, exp_len: %d", fc, len, exp_len);
                    ESP_LOG_BUFFER_HEX_LEVEL(TAG, buf_ptr, buf_len, ESP_LOG_WARN);
                    ESP_LOG_BUFFER_HEX_LEVEL(TAG, data_ptr, len, ESP_LOG_ERROR);
                }
//                ESP_LOGW(TAG, "%s::aps_user_payload_callback to dstEP %d from srcEP %d (Addr: " IEEE_ADDR_FMT ", shortAddr: %04x), ProfileID: %04x, ClusterID: %04x, dstAddrMode: %d, need_unlock: %s, time: %llu, dataLen: %d",
//                    Cmd::name, resp->dst_endpoint, resp->src_endpoint, IEEE_ADDR_PRINT(resp->dst_addr.addr_long), resp->dst_addr.addr_short, hdr->profile_id, hdr->cluster_id, resp->dst_addr_mode, resp->need_unlock ? "true" : "false", resp->tx_time, len);
            }
            auto tsn = data_ptr[1];
            auto req = ResolveStrategy::resolve(tsn);
            if (req) {
                ESP_LOGD(TAG, "%s::req_callback TSN: %d, status: %d", Cmd::name, tsn, ERROR_GET_CODE(status));
                if (status == ZB_APS_USER_PAYLOAD_CB_STATUS_SUCCESS && req->arg.hdr.dataLength == len) {
                    Cmd::send_response(*req, resp, sizeof(zb_apsde_data_confirm_t));
                } else {
                    report_failed(req->cmd, req->arg.hdr.dataLength != len ? static_cast<uint8_t>(ZB_ZDP_STATUS_TABLE_FULL) : ERROR_GET_CODE(status));
                }
                req->state.store(ResolveStrategy::state_t::S_NONE, std::memory_order_release);
            } else {
                ESP_LOGE(TAG, "%s::req_callback Request with TSN=%d not found: status: %d", Cmd::name, tsn, ERROR_GET_CODE(status));
            }
            zb_buf_free(buf);
        } else {
            ESP_LOGE(TAG, "%s::req_callback buffer failed", Cmd::name);
        }
    }
    static uint8_t get_dst_endpoint(zb_apsde_data_req_t& d) {
        if (d.dstAddrMode == 2 && IS_GROUP(d.addr.addr_long)) {
            d.dstAddrMode = 1;
            d.dstEndpoint = 0; // Group Endpoint
        }
        return d.dstEndpoint;
    }
    static uint8_t get_dst_endpoint(zb_apsde_data_req_nop_t& d) {
        if (d.dstAddrMode == 0 || d.dstAddrMode == 1) {
            return 0; // Group Endpoint
        }
        return 0xff; // Broadcast Endpoint
    }
    template <typename HdrVar> static zb_ret_t start_request(uint8_t buf, HdrVar& hdr, uint8_t* payload) {
        uint8_t dst_ep = get_dst_endpoint(hdr);
        zb_bool_t use_ack = (hdr.dstAddrMode == 2 || hdr.dstAddrMode == 3) ? ZB_TRUE : ZB_FALSE;
        char alias_buf[15] = "";
        if (hdr.useAlias) {
            snprintf(alias_buf, sizeof(alias_buf), ", Alias: %04x", hdr.aliasAddr);
        }
        ESP_LOGD(TAG, "%s::zb_aps_send_user_payload to dstEP %d from srcEP %d (Addr: " IEEE_ADDR_FMT ", shortAddr: %04x), ProfileID: %04x, ClusterID: %04x, dstAddrMode: %d, txOptions: 0x%02x, isAckEnabled: %s, dataLen: %d%s",
            Cmd::name, dst_ep, hdr.srcEndpoint, IEEE_ADDR_PRINT(hdr.addr.addr_long), hdr.addr.addr_short, hdr.profileID, hdr.clusterID, hdr.dstAddrMode, hdr.txOptions, use_ack ? "true" : "false", hdr.dataLength, alias_buf);
        zb_aps_set_user_data_tx_cb(Cmd::req_callback);
        if (unlikely(hdr.profileID == ZB_AF_GP_PROFILE_ID && hdr.clusterID == ZB_ZCL_CLUSTER_ID_GREEN_POWER && hdr.dataLength >= 10 && payload[2] == 0x01)) { // commandsResponse: pairing: { name: "pairing", ID: 0x01 }
            zb_ncp::delete_gp_device(payload, hdr.dataLength);
        }
        return zb_aps_send_user_payload(
            buf,
            hdr.addr,
            hdr.profileID,
            hdr.clusterID,
            dst_ep,
            hdr.srcEndpoint,
            hdr.dstAddrMode,
            use_ack,
            payload,
            (uint8_t)hdr.dataLength
        );
    }
    static void do_request(uint8_t buf, uint16_t req_idx) {
        auto& req = ResolveStrategy::get_by_index((uint8_t)req_idx);
        if (!buf) {
            ESP_LOGE(TAG, "%s::do_request failed (error: no buffer memory)", Cmd::name);
            report_failed(req.cmd, ZB_ZDP_STATUS_INSUFFICIENT_SPACE);
            req.state.store(ResolveStrategy::state_t::S_NONE, std::memory_order_release);
            return;
        }
        req.tsn = req.arg.data[1];
        zb_ret_t ret;
        if (req.arg.hdr.paramLength == 21) {
            ret = start_request(buf, req.arg.hdr, req.arg.data);
        } else {
            ret = start_request(buf, req.arg.hdr_nop, req.arg.data);
        }
        if (ret != RET_OK) {
            ESP_LOGE(TAG, "%s::do_request failed (error code: %d)", Cmd::name, ERROR_GET_CODE(ret));
            zb_buf_free(buf);
            report_failed(req.cmd, (ret == RET_INVALID_PARAMETER_3) ? ZB_ZDP_STATUS_NO_MATCH : ERROR_GET_CODE(ret));
            req.state.store(ResolveStrategy::state_t::S_NONE, std::memory_order_release);
            return;
        }
        req.state.store(ResolveStrategy::state_t::S_EXEC, std::memory_order_release);
        ESP_LOGD(TAG, "%s::do_request req_idx: %d, TSN: %d", Cmd::name, req_idx, req.tsn);
    }
    static bool check_arg_size(const void* buffer, uint16_t len) {
        if (len < sizeof(zb_apsde_data_req_nop_t)) return false;
        uint8_t paramLen = *static_cast<const uint8_t*>(buffer);
        if (paramLen == 21) {
            auto hdr = reinterpret_cast<const zb_apsde_data_req_t*>(buffer);
            return len == (sizeof(zb_apsde_data_req_t) + hdr->dataLength);
        } else if (paramLen == 20) {
            auto hdr = reinterpret_cast<const zb_apsde_data_req_nop_t*>(buffer);
            return len == (sizeof(zb_apsde_data_req_nop_t) + hdr->dataLength);
        }
        return false;
    }
    static void process(const zb_ncp::cmd_t& cmd, const void* buffer, uint16_t len) {
        if (!Cmd::check_arg_size(buffer, len)) {
            report_failed(cmd, ZB_ZDP_STATUS_INVALID_INDEX);
            return;
        }
        uint8_t req_idx;
        auto req = ResolveStrategy::start_resolve(cmd, &req_idx);
        if (!req) {
            report_failed(cmd, ZB_ZDP_STATUS_INSUFFICIENT_SPACE);
            return;
        }
        uint8_t paramLen = *static_cast<const uint8_t*>(buffer);
        uint8_t hdr_len;
        uint16_t data_len;
        if (paramLen == 21) {
            auto hdr = static_cast<const zb_apsde_data_req_t*>(buffer);
            hdr_len = sizeof(zb_apsde_data_req_t);
            data_len = hdr->dataLength;
            memcpy(&req->arg.hdr, hdr, hdr_len);
        } else {
            auto hdr = static_cast<const zb_apsde_data_req_nop_t*>(buffer);
            hdr_len = sizeof(zb_apsde_data_req_nop_t);
            data_len = hdr->dataLength;
            memcpy(&req->arg.hdr_nop, hdr, hdr_len);
        }
        if (data_len > 0) {
            if (data_len <= sizeof(req->arg.data)) {
                memcpy(req->arg.data, static_cast<const uint8_t*>(buffer) + hdr_len, data_len);
            } else {
                report_failed(cmd, ZB_ZDP_STATUS_INSUFFICIENT_SPACE);
                req->state.store(ResolveStrategy::state_t::S_NONE, std::memory_order_release);
                return;
            }
        }
        req->alloc_len = Cmd::get_request_alloc_size(req->arg);
        zb_ret_t ret;
        {
            utils::critical_section lock(&zb_ncp::m_mux_lock);
            ret = ZB_SCHEDULE_APP_CALLBACK(Cmd::buf_alloc, req_idx);
        }
        if (ret != RET_OK) {
            report_failed(cmd, ZB_ZDP_STATUS_INSUFFICIENT_SPACE);
            req->state.store(ResolveStrategy::state_t::S_NONE, std::memory_order_release);
            return;
        }
        ESP_LOGD(TAG, "%s::do_start req_idx: %d", Cmd::name, req_idx);
    }
};
