#include "../min_logger.h"

#if MIN_LOGGER_ENABLED && defined(MIN_LOGGER_BUFFERED_ESP32_PLATFORM)

    #include <driver/uart.h>
    #include <esp_log.h>
    #include <esp_netif.h>
    #include <esp_timer.h>
    #include <freertos/FreeRTOS.h>
    #include <freertos/event_groups.h>
    #include <freertos/task.h>
    #if MIN_LOGGER_ENABLE_UDP
        #include <lwip/inet.h>
        #include <lwip/sockets.h>
    #endif

    #include "lock_free_ring_buffer.h"

static const char* TAG = "min-logger";

    #ifdef __cplusplus
extern "C" {
    #endif

static_assert((MIN_LOGGER_BUFFER_SIZE & (MIN_LOGGER_BUFFER_SIZE - 1)) == 0,
              "MIN_LOGGER_BUFFER_SIZE must be power of two");
COREDUMP_DRAM_ATTR uint8_t min_logger_buffer[MIN_LOGGER_BUFFER_SIZE];
static constexpr uint8_t DATA_READY_BIT = (1 << 0);
static LockFreeRingBuffer ring_buffer(min_logger_buffer, MIN_LOGGER_BUFFER_SIZE);
static bool is_init = false;

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

    LockFreeRingBufferReader reader(&ring_buffer);
    LockFreeRingBufferReadResults results;

    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(parameters->hostname);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(parameters->port);
    bool udp_up = false;
    bool buffer_misaligned = false;

    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");

    int sock = -1;

    while (1) {
        if (!reader.PeekAvailable(&results)) {
            ESP_LOGE(TAG, "Fell behind");
            buffer_misaligned = true;
            continue;
        }
        // Check if a UDP packet's worth of data is ready to send.
        if (results.Size() >= udp_message_size) {
            // After dropping data, realign buffer.
            if (buffer_misaligned) {
                // The read tore, so move read head to start of buffer.
                if (results.part1_size < udp_message_size) {
                    if (!reader.MarkRead(results.part1_size)) {
                        ESP_LOGE(TAG, "Fell behind");
                    } else {
                        buffer_misaligned = false;
                    }
                    continue;
                }
            } else {
                // Since reads are always udp_message_size, and buffer is multiple
                // of udp_message_size, should never need to tear read.
                assert(results.part1_size >= udp_message_size);
            }

            // Open Socket if needed.
            if (sock == -1) {
                esp_netif_ip_info_t ip_info;
                // Check if networking is up.
                if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK && ip_info.ip.addr != 0) {
                    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
                }
            }

            int err = 0;
            if (sock != -1) {
                err = sendto(sock, results.part1, udp_message_size, 0, (struct sockaddr*)&dest_addr,
                             sizeof(dest_addr));
            }
            if (!reader.MarkRead(udp_message_size)) {
                ESP_LOGE(TAG, "Fell behind");
                buffer_misaligned = true;
            }
            if (err < 0) {
                if (udp_up) {
                    if (errno == 12) {
                        ESP_LOGW(TAG, "lwIP queue full. Is destination reachable?");
                    } else {
                        ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
                    }
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
    // buffer must be multiple of packet size.
    assert(MIN_LOGGER_BUFFER_SIZE > packet_size);
    assert(MIN_LOGGER_BUFFER_SIZE % packet_size == 0);
    // Can't init twice
    assert(!is_init);
    static UDPParameters parameters{.hostname = logging_udp_ip,
                                    .port = logging_udp_port,
                                    .poll_interval_ms = poll_interval_ms,
                                    .udp_message_size = packet_size};
    xTaskCreate(min_logger_udp_client_task, "min_logger_udp", 2048, &parameters, 1, NULL);
}

    #endif

static void min_logger_uart_task(void* pvParameters) {
    auto uart_num = *reinterpret_cast<const uart_port_t*>(pvParameters);
    LockFreeRingBufferReader reader(&ring_buffer);
    LockFreeRingBufferReadResults results;
    while (1) {
        if (!reader.PeekAvailable(&results)) {
            ESP_LOGE(TAG, "Fell behind");
            continue;
        }

        if (results.part1_size > 0) {
            uart_write_bytes(uart_num, results.part1, results.part1_size);
            if (results.part2_size > 0) {
                uart_write_bytes(uart_num, results.part2, results.part2_size);
            }
        }

        if (!reader.MarkRead(results.Size())) {
            ESP_LOGE(TAG, "Fell behind");
        }

        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void min_logger_init_uart(unsigned uart_num) {
    // Can't init twice
    assert(!is_init);
    static uart_port_t static_uart_num = (uart_port_t)uart_num;
    xTaskCreate(min_logger_uart_task, "min_logger_uart", 1024, &static_uart_num, 1, NULL);
}

void IRAM_ATTR min_logger_write(const uint8_t* msg, size_t len_bytes) {
    ring_buffer.Write(msg, len_bytes);
}

    #ifdef __cplusplus
}
    #endif

#endif