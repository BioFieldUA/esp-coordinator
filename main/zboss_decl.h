#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define ZB_COORDINATOR_ROLE
#define ZB_OSIF_CONFIGURABLE_TX_POWER
#define ZB_MAC_CONFIGURABLE_TX_POWER
#define ZB_LOW_SECURITY_MODE
#define ZB_CONTROL4_NETWORK_SUPPORT
#define ZB_MAC_INTERFACE_SINGLE
#define ZB_ENABLE_INTER_PAN_NON_DEFAULT_CHANNEL
#define ZB_CERTIFICATION_HACKS
#define ZB_TEST_GROUP_ALL
#define MAC_AUTO_DELAY_IN_MAC_GP_SEND
#define ZB_NWK_CHANNEL_ACCEPT_LEVEL 200
#define ZB_ENHANCED_BEACON_SUPPORT
#define ZB_JOINING_LIST_SUPPORT
#define ZB_PANID_TABLE_SIZE 28
#define ZB_NWK_BRR_TABLE_SIZE 16
#define ZB_CONFIG_OVERALL_NETWORK_SIZE 128
#define ZB_CONFIG_HIGH_TRAFFIC
#define ZB_CONFIG_APPLICATION_COMPLEX

#include "zboss_api.h"
#include "zboss_api_zcl.h"
#include "zboss_api_zgp.h"
#include "zboss_api_zdo.h"
#include "zboss_api_aps.h"
#include "zb_types.h"
#include "zgp/zgp_internal.h"
#include "zcl/zb_zcl_basic.h"
#include "zcl/zb_zcl_device_management.h"
#include "zcl/esp_zigbee_zcl_ota.h"

#ifdef __cplusplus
}
#endif
