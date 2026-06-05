#pragma once
#include <cstdint>
#include <esp_err.h>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

class protocol {
public:
    struct zb_transport_header_t {
        uint8_t signature[2];
        uint16_t packet_len;
        uint8_t packet_type;
        uint8_t is_ack : 1;
        uint8_t is_nack : 1;
        uint8_t packet_seq : 2;
        uint8_t ack_seq : 2;
        uint8_t first_fragment : 1;
        uint8_t last_fragment : 1;
        uint8_t header_crc;
    } __attribute__((packed)) __attribute__((aligned(1)));
    protocol(const protocol&) = delete;
    protocol& operator=(const protocol&) = delete;
    protocol(protocol&&) = delete;
    protocol& operator=(protocol&&) = delete;
    static esp_err_t start();
    static inline esp_err_t on_receive_data(const uint8_t* data, uint16_t size) { return instance().on_receive_data_impl(data, size); }
    static inline esp_err_t send_data(const void* data, uint16_t size) { return instance().send_data_impl(data, size); }
    static esp_err_t send_wrong_ack();
private:
    static constexpr uint16_t RINGBUF_SIZE = 20480;
    static constexpr uint8_t ZBOSS_NCP_API_HL = 0x06;
    static constexpr uint8_t next_seq_map[4] = { 0x01, 0x02, 0x03, 0x01 };
    protocol();
    ~protocol();
    static protocol& instance();
    static uint8_t next_seq(uint8_t seq);
    static uint8_t* find_signature(uint8_t* s, const uint8_t* end);
    esp_err_t run();
    void send_ack(const zb_transport_header_t& hdr);
    void send_nack(const zb_transport_header_t& hdr);
    void on_frame(const zb_transport_header_t& hdr, const void* data, uint16_t size);
    esp_err_t on_receive_data_impl(const uint8_t* data, uint16_t size);
    esp_err_t send_data_impl(const void* data, uint16_t size);
    std::atomic<uint8_t> m_tx_seq;
    uint16_t m_rx_buffer_pos;
    static inline uint8_t m_rx_buffer[RINGBUF_SIZE];
};
