#pragma once
#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

class app final {
public:
    enum event_t : uint8_t {
        EVENT_INPUT,                    // Input event from NCP to host
        EVENT_OUTPUT,                   // Output event from host to NCP
        EVENT_RESET,                    // Reset event from host to NCP
    };
    struct ctx_t {
        event_t event;
        uint16_t size;
        uint8_t* buf_ptr;
    };
    app(const app&) = delete;
    app& operator=(const app&) = delete;
    app(app&&) = delete;
    app& operator=(app&&) = delete;
    static esp_err_t start();
    static inline esp_err_t send_event(const ctx_t& ctx) { return instance().send_event_impl(ctx); }
private:
    static constexpr uint16_t EVENT_QUEUE_LEN = 256;
    app();
    ~app();
    static app& instance();
    esp_err_t run();
    esp_err_t process_event(const ctx_t& ctx);
    esp_err_t send_event_impl(const ctx_t& ctx);
    QueueHandle_t m_queue{ nullptr };
};
