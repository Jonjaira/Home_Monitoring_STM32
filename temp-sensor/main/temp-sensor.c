/* BSD Socket API Example

 This example code is in the Public Domain (or CC0 licensed, at your option.)

 Unless required by applicable law or agreed to in writing, this
 software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 CONDITIONS OF ANY KIND, either express or implied.
 */
#include <string.h>
#include <sys/param.h>
#include <errno.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/portmacro.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "driver/gpio.h"
#include "driver/hw_timer.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0

#define GPIO_SENSOR_DATA_LINE  2
#define GPIO_OUTPUT_PIN_SEL    (1ULL << GPIO_SENSOR_DATA_LINE)
#define GPIO_INPUT_PIN_SEL     (1ULL << GPIO_SENSOR_DATA_LINE)

typedef union dht11_input
{
    struct dht11_input_bytes
    {
        uint8_t humidity_percent_integer;
        uint8_t humidity_percent_decimal;
        uint8_t temp_celsius_integer;
        uint8_t temp_celsius_decimal;
        uint8_t checksum;
    }dht11_input;
    uint8_t bytes[sizeof(struct dht11_input_bytes)];
}dht11_input_t;

typedef struct dh11_data
{
    uint8_t sensor_humidity;
    uint8_t sensor_temperature;
}dh11_data_t;

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

static const char *TAG = "\e[38;5;208mSensor\e[0m";
static xQueueHandle gpio_evt_queue = NULL;

static dh11_data_t sensor_data = {0};

static SemaphoreHandle_t xSemaphore = NULL;

// Define your custom log function
int my_log_putchar(int ch)
{
    putchar((char)ch);
    return ch;
}

void hw_timer_callback(void *arg)
{
    int32_t value = -1;
    xQueueSendFromISR(gpio_evt_queue, &value, NULL);
}

static void gpio_data_line_isr_handler(void *arg)
{
    // Disarm the timer to stop counting
    hw_timer_enable(false);

    // Get the current timer count, which represents the time since the last rearming
    int32_t current_count = (int32_t)hw_timer_get_count_data();

    // Send the count to the task
    xQueueSendFromISR(gpio_evt_queue, &current_count, NULL);

    // Rearm the timer to reset the count and start timing again
    hw_timer_alarm_us(2000, false); // Assuming 1ms max interval
}

static dh11_data_t dh11_temp_humid_get(void)
{
    dh11_data_t tmp = {0};

    if(xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE)
    {
        tmp = sensor_data;
        xSemaphoreGive(xSemaphore);
    }

    return tmp;
}

static void dth11_task(void *pvParameters)
{
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);
    gpio_set_level(GPIO_SENSOR_DATA_LINE, 1);

    //install gpio isr service
    gpio_install_isr_service(0);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_SENSOR_DATA_LINE, gpio_data_line_isr_handler, NULL);

    //create a queue to handle gpio event from isr
    gpio_evt_queue = xQueueCreate(50, sizeof(int32_t));

    xSemaphore = xSemaphoreCreateMutex();

    // Initialize but do not start the hardware timer yet
    hw_timer_init(hw_timer_callback, NULL);

    printf("\e[2K%s: DTH11 Sensor Task Started\r\n", TAG);

    vTaskDelay(5000 / portTICK_RATE_MS);

    while(1)
    {
        dht11_input_t parsed_dht11_data = {0};
        int32_t data_line_sampling_buffer[42] = {0};

        bool receiving = true;
        int32_t timer_value = 0;
        uint8_t bit = 0;

        taskENTER_CRITICAL();
        gpio_set_level(GPIO_SENSOR_DATA_LINE, 0);
        for (uint32_t i = 0; i < 261900; i++);
        gpio_set_level(GPIO_SENSOR_DATA_LINE, 1);
        taskEXIT_CRITICAL();

        /* Now we immediately configure the line to be input */
        io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pull_down_en = 1;
        io_conf.pull_up_en = 0;
        io_conf.intr_type = GPIO_INTR_NEGEDGE;
        gpio_config(&io_conf);

        while(receiving)
        {
            xQueueReceive(gpio_evt_queue, &timer_value, portMAX_DELAY);

            if (timer_value != -1)
            {
                data_line_sampling_buffer[bit++] = timer_value;

                if (bit == sizeof(data_line_sampling_buffer))
                {
                    receiving = false;
                }
            }
            else
            {
                receiving = false;
            }
        }

        /* Now we immediately configure the line to be output again for IDLE line */
        io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
        io_conf.pull_down_en = 0;
        io_conf.pull_up_en = 1;
        io_conf.mode = GPIO_MODE_OUTPUT;
        gpio_config(&io_conf);

        hw_timer_enable(false);

        if (bit == 42)
        {
            printf("\e[J\r\n╭─ \e[1mDHT11 Bit Values\e[0m ────╮\r\n");
            for (uint32_t i = 0; i < bit; i++)
            {
                printf("│ Bit[\e[96m%2d\e[0m] = \e[93m%4d\e[0m -> (\e[92m%d\e[0m) │\r\n",
                       i,
                       data_line_sampling_buffer[i] > 10000? 0 : data_line_sampling_buffer[i],
                       data_line_sampling_buffer[i] > 9650? 0 : 1);
            }
            printf("╰───────────────────────╯\r\n\r\n");

            uint8_t sampling_buffer_index = 2;

            for (int byte_index = 0; byte_index < sizeof(dht11_input_t); byte_index++)
            {
                for (int bit_index = 8; bit_index > 0 ; bit_index--)
                {
                    uint8_t parse_bit = data_line_sampling_buffer[sampling_buffer_index++] > 9650? 0 : 1;
                    parsed_dht11_data.bytes[byte_index] |= parse_bit << (bit_index - 1);
                }
            }

            uint8_t calc_checksum = parsed_dht11_data.dht11_input.humidity_percent_integer;
            calc_checksum += parsed_dht11_data.dht11_input.humidity_percent_decimal;
            calc_checksum += parsed_dht11_data.dht11_input.temp_celsius_integer;
            calc_checksum += parsed_dht11_data.dht11_input.temp_celsius_decimal;

            if (calc_checksum == parsed_dht11_data.dht11_input.checksum)
            {
                if(xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE)
                {
                    sensor_data.sensor_humidity = parsed_dht11_data.dht11_input.humidity_percent_integer;
                    printf("   Humidity: \e[93m%d\e[0m\r\n", sensor_data.sensor_humidity);

                    sensor_data.sensor_temperature = parsed_dht11_data.dht11_input.temp_celsius_integer;
                    printf("Temperature: \e[93m%d\e[0m\r\n\e[48A", sensor_data.sensor_temperature);

                    xSemaphoreGive(xSemaphore);
                }
            }
            else
            {
                printf("   Humidity: \e[91m%d\e[0m (received %02X - calc %02X)\r\n",
                       parsed_dht11_data.dht11_input.humidity_percent_integer, parsed_dht11_data.dht11_input.checksum, calc_checksum);
                printf("Temperature: \e[91m%d\e[0m (received %02X - calc %02X)\r\n\e[48A",
                       parsed_dht11_data.dht11_input.temp_celsius_integer, parsed_dht11_data.dht11_input.checksum, calc_checksum);
            }
        }

        vTaskDelay(1000 / portTICK_RATE_MS);
    }
}

static void tcp_client_task(void *pvParameters)
{
    char rx_buffer[128];
    char addr_str[128];
    int addr_family;
    int ip_protocol;

    while (1)
    {
        struct sockaddr_in destAddr;
        destAddr.sin_addr.s_addr = inet_addr(CONFIG_TS_IPV4_ADDR);
        destAddr.sin_family = AF_INET;
        destAddr.sin_port = htons(CONFIG_TS_SENSOR_PORT);
        addr_family = AF_INET;
        ip_protocol = IPPROTO_IP;
        inet_ntoa_r(destAddr.sin_addr, addr_str, sizeof(addr_str) - 1);

        int sock = socket(addr_family, SOCK_STREAM, ip_protocol);
        if (sock < 0)
        {
            ESP_LOGE(TAG, "Unable to create socket: errno \e[91m%d\e[0m", errno);
            break;
        }
        ESP_LOGI(TAG, "Socket created");

        int err = connect(sock, (struct sockaddr* )&destAddr, sizeof(destAddr));
        if (err != 0)
        {
            ESP_LOGE(TAG, "Socket unable to connect: errno \e[91m%d\e[0m", errno);
            close(sock);

            vTaskDelay(3000 / portTICK_PERIOD_MS);

            continue;
        }
        printf("\e[2K\e[1;92mSuccessfully connected to \e[1;93m%s\e[1;92m:\e[1;93m%d\e[0m\r\n\e[2K",
               CONFIG_TS_IPV4_ADDR,
               CONFIG_TS_SENSOR_PORT);

        while (1)
        {
            memset(rx_buffer, 0, sizeof(rx_buffer));

            int len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
            // Error occured during receiving
            if (len < 0)
            {
                ESP_LOGE(TAG, "recv failed: errno \e[91m%d\e[0m", errno);
                break;
            }
            // Data received
            else
            {
                rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string
                printf("\e[2KCommand Received from \e[93m%s\e[0m: \e[1;96m%s\e[0m\e[2K", addr_str, rx_buffer);

                if (strcmp("READ\n", rx_buffer) == 0)
                {
                    dh11_data_t sens_dat = dh11_temp_humid_get();

                    char response[128] = {0};

                    sprintf(response,
                            "{\"BedRoom\":{\"TempSensor\":{\"Temperature\":%d,\"Humidity\":%d}}}\n",
                            sens_dat.sensor_temperature,
                            sens_dat.sensor_humidity);

                    printf("\e[2KResponse: \e[93m%s\e[0m\e[2K", response);

                    int err = send(sock, response, strlen(response), 0);
                    if (err < 0)
                    {
                        ESP_LOGE(TAG, "Error occured during sending: errno \e[92%d\e[0m", errno);
                        break;
                    }
                }
                else
                {
                    printf("\e[2KCommand \e[93m%s\e[0m\e[2K NOT Supported\r\n\e[2K", rx_buffer);
                    for (int i = 0; i < strlen(rx_buffer); i++)
                    {
                        printf("%2X ", rx_buffer[i]);
                    }
                    printf("\r\n\e[2K");
                }
            }

            vTaskDelay(2000 / portTICK_PERIOD_MS);
        }

        if (sock != -1)
        {
            ESP_LOGE(TAG, "Shutting down socket and restarting...");
            shutdown(sock, 0);
            close(sock);
        }
    }
    vTaskDelete(NULL);
}

static void event_handler(void *arg,
                          esp_event_base_t event_base,
                          int32_t event_id,
                          void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        ESP_LOGE(TAG, "Connect to the AP fail");
        ESP_LOGE(TAG, "Retry to connect to the AP...");
        esp_wifi_connect();
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t*)event_data;
        ESP_LOGI(TAG, "Got ip: \e[1;93m%s\e[0;39m", ip4addr_ntoa(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

bool wifi_init_sta(void)
{
    bool is_connected = false;

    s_wifi_event_group = xEventGroupCreate();

    tcpip_adapter_init();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    wifi_config_t wifi_config = {.sta = {.ssid = CONFIG_TS_WIFI_SSID,
                                         .password = CONFIG_TS_WIFI_PASSWORD}, };

    /* Setting a password implies station will connect to all security modes including WEP/WPA.
     * However these modes are deprecated and not advisable to be used. Incase your Access point
     * doesn't support WPA2, these mode can be enabled by commenting below line */

    if (strlen((char*)wifi_config.sta.password))
    {
        wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "\e[96mwifi_init_sta finished.\e[0m");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT ,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(TAG,
                 "connected to AP SSID:\e[1;93m%s\e[0m password:\e[1;93m%s\e[0m",
                 CONFIG_TS_WIFI_SSID,
                 CONFIG_TS_WIFI_PASSWORD);

        is_connected = true;
    }
    else
    {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler));
    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler));
    vEventGroupDelete(s_wifi_event_group);

    return is_connected;
}

void app_main()
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Set your custom log function
    esp_log_set_putchar(my_log_putchar);

    if (!wifi_init_sta())
    {
        while(1);
    }

    /* Higher priority = higher number */
    xTaskCreate(dth11_task,      "DTH11 Driver", 4096, NULL, 6, NULL);
    xTaskCreate(tcp_client_task, "TCP Client",   4096, NULL, 5, NULL);
}
