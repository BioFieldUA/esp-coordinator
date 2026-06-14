#include "transport.h"
#include "app.h"
#include "protocol.h"
#include "utils.h"
#include "esp_log.h"
#include "sdkconfig.h"

#if defined(CONFIG_NCP_BUS_MODE_UART)
#include <driver/uart.h>
static constexpr uart_port_t UART_PORT_NUM = static_cast<uart_port_t>(CONFIG_NCP_BUS_UART_NUM);
#elif defined(CONFIG_NCP_BUS_MODE_USB)
#include <driver/usb_serial_jtag.h>
#endif

static const char* TAG = "TRANSPORT";

transport& transport::instance() {
    static transport s_transport;
    return s_transport;
}

transport::transport() {
    esp_err_t ret = ESP_OK;
#if defined(CONFIG_NCP_BUS_MODE_UART)
    uart_config_t uart_config = {
        .baud_rate = CONFIG_NCP_BUS_UART_BAUD_RATE,
        .data_bits = static_cast<uart_word_length_t>(CONFIG_NCP_BUS_UART_BYTE_SIZE),
        .parity = static_cast<uart_parity_t>(CONFIG_NCP_BUS_UART_PARITY),
        .stop_bits = static_cast<uart_stop_bits_t>(CONFIG_NCP_BUS_UART_STOP_BITS),
        .flow_ctrl = static_cast<uart_hw_flowcontrol_t>(CONFIG_NCP_BUS_UART_FLOW_CONTROL),
        .rx_flow_ctrl_thresh = static_cast<uint8_t>((CONFIG_NCP_BUS_UART_FLOW_CONTROL & 1) ? 122 : 0),
        .source_clk = UART_SCLK_DEFAULT,
        .flags = {
            .allow_pd = 0,
            .backup_before_sleep = 0
        } 
    };
    ret = uart_driver_install(UART_PORT_NUM, BUF_SIZE, BUF_SIZE, 32, &m_uart_queue, 0);
    if (ret == ESP_OK) ret = uart_param_config(UART_PORT_NUM, &uart_config);
    if (ret == ESP_OK) ret = uart_set_pin(
        UART_PORT_NUM,
        (CONFIG_NCP_BUS_UART_TX_PIN < 0) ? TX : CONFIG_NCP_BUS_UART_TX_PIN,
        (CONFIG_NCP_BUS_UART_RX_PIN < 0) ? RX : CONFIG_NCP_BUS_UART_RX_PIN,
        CONFIG_NCP_BUS_UART_RTS_PIN,
        CONFIG_NCP_BUS_UART_CTS_PIN
    );
#elif defined(CONFIG_NCP_BUS_MODE_USB)
    usb_serial_jtag_driver_config_t usb_config = {
        .tx_buffer_size = BUF_SIZE,
        .rx_buffer_size = BUF_SIZE
    };
    ret = usb_serial_jtag_driver_install(&usb_config);
#else
#error "Unsupported transport"
#endif
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Initialization Failed, error: %s", esp_err_to_name(ret));
    } else {
        m_stop_sem = xSemaphoreCreateBinary();
        if (!m_stop_sem) {
            ESP_LOGE(TAG, "Initialization Failed, error: Stop Semaphore is not created properly");
        }
    }
}

transport::~transport() {
    if (m_task_handle && m_stop_sem) {
        xTaskNotifyGive(m_task_handle);
#if defined(CONFIG_NCP_BUS_MODE_UART)
        uart_event_t dummy_event = {};
        dummy_event.type = UART_EVENT_MAX;
        xQueueSend(m_uart_queue, &dummy_event, 0);
#elif defined(CONFIG_NCP_BUS_MODE_USB)
        xTaskAbortDelay(m_task_handle);
#endif
        if (xSemaphoreTake(m_stop_sem, pdMS_TO_TICKS(2000)) == pdTRUE) {
            ESP_LOGI(TAG, "Task deleted");
        } else {
            ESP_LOGE(TAG, "Task hang! Forced deletion.");
            vTaskDelete(m_task_handle);
        }
    }
    m_task_handle = nullptr;
#if defined(CONFIG_NCP_BUS_MODE_UART)
    uart_driver_delete(UART_PORT_NUM);
#elif defined(CONFIG_NCP_BUS_MODE_USB)
    usb_serial_jtag_driver_uninstall();
#endif
    if (m_stop_sem) {
        vSemaphoreDelete(m_stop_sem);
        m_stop_sem = nullptr;
    }
}

int transport::write(const void* buffer, uint16_t size) {
#if defined(CONFIG_NCP_BUS_MODE_UART)
    return uart_write_bytes(UART_PORT_NUM, buffer, size);
#elif defined(CONFIG_NCP_BUS_MODE_USB)
    return usb_serial_jtag_write_bytes(buffer, size, portMAX_DELAY);
#endif
}

void transport::task_impl() {
    ESP_LOGI(TAG, "Task Started");
#if defined(CONFIG_NCP_BUS_MODE_UART)
    uart_event_t event;
#endif
    while (ulTaskNotifyTake(pdTRUE, 0) == 0) {
        size_t data_len = 0;
        uint8_t* rx_buf = nullptr;
#if defined(CONFIG_NCP_BUS_MODE_UART)
        if (likely(xQueueReceive(m_uart_queue, &event, portMAX_DELAY))) {
            switch (event.type) {
            case UART_DATA:
                data_len = event.size;
                break;
            case UART_BUFFER_FULL:
                ESP_LOGE(TAG, "UART Buffer overflow");
                data_len = event.size;
                break;
            case UART_FIFO_OVF:
                ESP_LOGE(TAG, "UART FIFO overflow");
                uart_get_buffered_data_len(UART_PORT_NUM, &data_len);
                break;
            default:
                continue;
            }
            if (likely(data_len > 0)) {
                rx_buf = static_cast<uint8_t*>(malloc(data_len));
                if (likely(rx_buf)) {
                    int read_bytes = uart_read_bytes(UART_PORT_NUM, rx_buf, data_len, 0);
                    if (unlikely(read_bytes <= 0)) {
                        free(rx_buf);
                        rx_buf = nullptr;
                    } else {
                        data_len = read_bytes;
                    }
                } else {
                    ESP_LOGE(TAG, "No memory for malloc");
                }
            }
        }
#elif defined(CONFIG_NCP_BUS_MODE_USB)
        rx_buf = static_cast<uint8_t*>(malloc(BUF_SIZE));
        if (likely(rx_buf)) {
            int read_bytes = usb_serial_jtag_read_bytes(rx_buf, BUF_SIZE, portMAX_DELAY); // pdMS_TO_TICKS(10)
            if (unlikely(read_bytes <= 0)) {
                free(rx_buf);
                rx_buf = nullptr;
            } else {
                data_len = read_bytes;
                rx_buf = static_cast<uint8_t*>(realloc(rx_buf, data_len));
            }
        } else {
            ESP_LOGE(TAG, "No memory for malloc transport rx_buffer");
        }
#endif
        if (likely(rx_buf)) {
            app::ctx_t ncp_event = {
                .event = app::EVENT_OUTPUT,
                .size = static_cast<uint16_t>(data_len),
                .buf_ptr = rx_buf
            };
            if (unlikely(app::send_event(ncp_event) != ESP_OK)) {
                ESP_LOGE(TAG, "Failed to receive data, dropping data");
                free(rx_buf);
            }
        }
    }
    ESP_LOGI(TAG, "Task Stoped");
    if (m_stop_sem) xSemaphoreGive(m_stop_sem);
    vTaskDelete(NULL);
}

esp_err_t transport::process_output(void* buffer, uint16_t size) {
    if (unlikely(!buffer)) return ESP_ERR_INVALID_ARG;
    if (unlikely(size == 0)) {
        free(buffer);
        return ESP_OK;
    }
    esp_err_t ret = protocol::on_receive_data(static_cast<uint8_t*>(buffer), size);
    free(buffer);
    return ret;
}

esp_err_t transport::process_input(void* buffer, uint16_t size) {
    if (unlikely(!buffer)) return ESP_ERR_INVALID_ARG;
    if (unlikely(size == 0)) {
        free(buffer);
        return ESP_OK;
    }
    uint8_t* data_ptr = static_cast<uint8_t*>(buffer);
    int remaining = size;
    while (remaining > 0) {
        int written = write(data_ptr, remaining);
        if (unlikely(written < 0)) {
            free(buffer);
            return ESP_ERR_NOT_FINISHED;
        } else {
            data_ptr += written;
            remaining -= written;
            if (unlikely(remaining > 0)) {
                portYIELD();
            }
        }
    }
    free(buffer);
    return ESP_OK;
}

esp_err_t transport::send(void* data, uint16_t size) {
    if (unlikely(!data)) return ESP_ERR_INVALID_ARG;
    if (unlikely(size == 0)) {
        free(data);
        return ESP_OK;
    }
    app::ctx_t ncp_event = {
        .event = app::EVENT_INPUT,
        .size = size,
        .buf_ptr = static_cast<uint8_t*>(data)
    };
    return app::send_event(ncp_event);
}

esp_err_t transport::run() {
    if (!m_stop_sem || m_task_handle != nullptr) return ESP_ERR_INVALID_STATE;
    return (xTaskCreate(&task, "transport", TASK_STACK, this, TASK_PRIORITY, &m_task_handle) == pdTRUE) ? ESP_OK : ESP_FAIL;
}

esp_err_t transport::start() {
    ESP_LOGI(TAG, "Starting...");
    return instance().run();
}
