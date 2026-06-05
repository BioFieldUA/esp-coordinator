#pragma once
#include <cstdint>
#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

class transport {
public:
    transport(const transport&) = delete;
    transport& operator=(const transport&) = delete;
    transport(transport&&) = delete;
    transport& operator=(transport&&) = delete;
    static esp_err_t start();
    static esp_err_t process_output(void* buffer, uint16_t size);
    static esp_err_t process_input(void* buffer, uint16_t size);
    static esp_err_t send(void* data, uint16_t size);
private:
    static constexpr uint16_t BUF_SIZE = 1024;
    static constexpr uint16_t TASK_STACK = 8192;
    static constexpr uint16_t TASK_PRIORITY = 18;
    transport();
    ~transport();
    static transport& instance();
    esp_err_t run();
    static int write(const void* data, uint16_t size);
    void task_impl();
    static inline void task(void* pvParameter) { static_cast<transport*>(pvParameter)->task_impl(); }
    SemaphoreHandle_t m_stop_sem{ nullptr };
    TaskHandle_t m_task_handle{ nullptr };
#ifdef CONFIG_NCP_BUS_MODE_UART
    QueueHandle_t m_uart_queue;
#endif
};
