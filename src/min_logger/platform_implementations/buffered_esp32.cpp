#include "min_logger.h"

#if MIN_LOGGER_ENABLED && defined(MIN_LOGGER_BUFFERED_ESP32_PLATFORM)

    #include <driver/uart.h>
    #include <esp_log.h>
    #include <esp_netif.h>
    #include <esp_timer.h>
    #include <freertos/FreeRTOS.h>
    #include <freertos/ringbuf.h>
    #include <freertos/task.h>
    #if MIN_LOGGER_ENABLE_UDP
        #include <lwip/inet.h>
        #include <lwip/sockets.h>
    #endif

static const char* TAG = "min-logger";

    #ifdef __cplusplus
extern "C" {
    #endif

COREDUMP_DRAM_ATTR uint8_t min_logger_buffer[MIN_LOGGER_BUFFER_SIZE];

static size_t dummy_ring_buffer_offset = 0;
static portMUX_TYPE dummy_ring_buffer_offset_mutex = portMUX_INITIALIZER_UNLOCKED;
static StaticRingbuffer_t ring_buffer_struct;
static RingbufHandle_t min_logger_rtos_ring = nullptr;

static void IRAM_ATTR write_dummy_ring_buffer(const void* msg, size_t msg_size) {
    assert(msg_size < MIN_LOGGER_BUFFER_SIZE);

    // Update write offset.
    taskENTER_CRITICAL(&dummy_ring_buffer_offset_mutex);
    void* write_ptr = ((char*)min_logger_buffer) + dummy_ring_buffer_offset;
    size_t free_size = MIN_LOGGER_BUFFER_SIZE - dummy_ring_buffer_offset;
    if (free_size < msg_size) {
        dummy_ring_buffer_offset = msg_size - free_size;
    } else {
        dummy_ring_buffer_offset += msg_size;
    }
    taskEXIT_CRITICAL(&dummy_ring_buffer_offset_mutex);

    // Since the offset has been updated, can write data outside of critical section.

    // Wrap message around end of buffer.
    if (free_size < msg_size) {
        memcpy(write_ptr, msg, free_size);
        write_ptr = min_logger_buffer;
        msg = ((const char*)msg) + free_size;
        msg_size -= free_size;
    }

    memcpy(write_ptr, msg, msg_size);
}

static void init_ring_buffer(size_t buffer_size) {
    min_logger_rtos_ring = xRingbufferCreateStatic(buffer_size, RINGBUF_TYPE_BYTEBUF,
                                                   min_logger_buffer, &ring_buffer_struct);
}

    #if MIN_LOGGER_ENABLE_UDP

struct UDPParameters {
    const char* hostname;
    uint16_t port;
    unsigned poll_interval_ms;
    size_t udp_message_size;
};

// https://github.com/espressif/esp-idf/tree/master/examples/protocols/sockets/udp_client
static void min_logger_udp_client_task(void* pvParameters) {
    auto parameters = reinterpret_cast<const UDPParameters*>(pvParameters);
    const size_t udp_message_size = parameters->udp_message_size;

    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(parameters->hostname);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(parameters->port);
    bool udp_up = false;

    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");

    int sock = -1;

    while (1) {
        // Check if a UDP packet's worth of data is ready to send.
        if (xRingbufferGetCurFreeSize(min_logger_rtos_ring) <= udp_message_size) {
            // Open Socket if needed.
            if (sock == -1) {
                esp_netif_ip_info_t ip_info;
                // Check if networking is up.
                if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK && ip_info.ip.addr != 0) {
                    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
                }
            }

            // Even if socket is down, still need to drain buffer.
            size_t read_size = 0;
            // By always reading half the buffer size, the read will never be limitted by
            // rolling over the end of the buffer.
            void* held_data = xRingbufferReceiveUpTo(
                min_logger_rtos_ring, &read_size, pdMS_TO_TICKS(portMAX_DELAY), udp_message_size);
            assert(read_size == udp_message_size);

            int err = 0;
            if (sock != -1) {
                err = sendto(sock, held_data, udp_message_size, 0, (struct sockaddr*)&dest_addr,
                             sizeof(dest_addr));
            }
            vRingbufferReturnItem(min_logger_rtos_ring, held_data);
            if (err < 0) {
                if (udp_up) {
                    ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
                    udp_up = false;
                }
                shutdown(sock, 0);
                close(sock);
                sock = -1;
            } else if (sock != -1 && !udp_up) {
                ESP_LOGE(TAG, "UDP client up");
                udp_up = true;
            }
        }
        vTaskDelay(parameters->poll_interval_ms / portTICK_PERIOD_MS);
    }
}

void min_logger_init_udp(size_t packet_size, unsigned poll_interval_ms, const char* logging_udp_ip,
                         uint16_t logging_udp_port) {
    // Buffer must be divisible by two
    assert(MIN_LOGGER_BUFFER_SIZE >= packet_size * 2);
    // Can't init twice
    assert(min_logger_rtos_ring == nullptr);
    static UDPParameters parameters{.hostname = logging_udp_ip,
                                    .port = logging_udp_port,
                                    .poll_interval_ms = poll_interval_ms,
                                    .udp_message_size = packet_size};
    init_ring_buffer(packet_size * 2);
    xTaskCreate(min_logger_udp_client_task, "min_logger_udp", 2048, &parameters, 1, NULL);
}

    #endif

static void min_logger_uart_task(void* pvParameters) {
    auto uart_num = *reinterpret_cast<const uart_port_t*>(pvParameters);
    while (1) {
        size_t read_size = 0;
        void* held_data =
            xRingbufferReceive(min_logger_rtos_ring, &read_size, pdMS_TO_TICKS(portMAX_DELAY));

        uart_write_bytes(uart_num, held_data, read_size);
        vRingbufferReturnItem(min_logger_rtos_ring, held_data);
    }
}

void min_logger_init_uart(unsigned uart_num) {
    // Can't init twice
    assert(min_logger_rtos_ring == nullptr);
    static uart_port_t static_uart_num = (uart_port_t)uart_num;
    init_ring_buffer(MIN_LOGGER_BUFFER_SIZE);
    xTaskCreate(min_logger_uart_task, "min_logger_uart", 1024, &static_uart_num, 1, NULL);
}

    #ifdef __cplusplus
}
    #endif

#endif