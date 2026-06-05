#include "app.h"
#include "storage.h"
#include "transport.h"
#include "protocol.h"
#include "zb_ncp.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "utils.h"

static const char* TAG = "APP";

app& app::instance() {
    static app s_app;
    return s_app;
}

app::app() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        ret = nvs_flash_init();
    }
    if (ret == ESP_OK) m_queue = xQueueCreate(EVENT_QUEUE_LEN, sizeof(ctx_t));
    if (!m_queue) {
        ESP_LOGE(TAG, "Initialization Failed, error: %s", esp_err_to_name(ret));
    }
}

app::~app() {
    if (m_queue) {
        vQueueDelete(m_queue);
        m_queue = nullptr;
    }
}

esp_err_t app::send_event_impl(const ctx_t& ctx) {
    if (!m_queue) return ESP_ERR_INVALID_STATE;
    BaseType_t ret = pdTRUE;
    if (xPortInIsrContext() == pdTRUE) {
        BaseType_t xTaskWoken = pdFALSE;
        ret = xQueueSendFromISR(m_queue, &ctx, &xTaskWoken);
        if (xTaskWoken == pdTRUE) {
            portYIELD_FROM_ISR();
        }
    } else {
        ret = xQueueSend(m_queue, &ctx, 0);
    }
    return (ret == pdTRUE) ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t app::process_event(const ctx_t& ctx) {
    switch (ctx.event) {
    case EVENT_INPUT:
        return transport::process_input(ctx.buf_ptr, ctx.size);
    case EVENT_OUTPUT:
        return transport::process_output(ctx.buf_ptr, ctx.size);
    case EVENT_RESET:
        ESP_LOGI(TAG, "ESP32 Restart");
        vTaskDelay(pdMS_TO_TICKS(ctx.size));
        esp_restart();
        return ESP_OK;
    default:
        return ESP_ERR_INVALID_ARG;
    }
}

esp_err_t app::run() {
    if (!m_queue) return ESP_ERR_INVALID_STATE;
    esp_err_t ret = storage::init();
    if (ret != ESP_OK) return ret;
    ret = protocol::start();
    if (ret != ESP_OK) return ret;
    ret = transport::start();
    if (ret != ESP_OK) return ret;
    ret = zb_ncp::init();
    if (ret != ESP_OK) return ret;
    ctx_t ctx;
    while (ret == ESP_OK) {
        if (xQueueReceive(m_queue, &ctx, portMAX_DELAY) == pdTRUE) {
            if ((ret = process_event(ctx)) != ESP_OK) {
                ESP_LOGE(TAG, "Event processing Failed, error: %s", esp_err_to_name(ret));
            }
        }
    }
    return ret;
}

esp_err_t app::start() {
    utils::init_external_antenna(TAG);
    ESP_LOGI(TAG, "Starting ZB Coordinator Version: %d.%d.%d.%d", (PROJECT_VER_UINT32 >> 24) & 0xFF, (PROJECT_VER_UINT32 >> 16) & 0xFF, (PROJECT_VER_UINT32 >> 8) & 0xFF, PROJECT_VER_UINT32 & 0xFF);
    return instance().run();
}
