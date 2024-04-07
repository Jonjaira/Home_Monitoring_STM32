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

#define GPIO_DEBUG_LINE        0
#define GPIO_SENSOR_DATA_LINE  2
#define GPIO_OUTPUT_PIN_SEL    (1ULL << GPIO_DEBUG_LINE)
#define GPIO_INPUT_PIN_SEL     (1ULL << GPIO_SENSOR_DATA_LINE)

#define SENSOR_SAMPLE_NUM_MAX  26

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

static const char *TAG = "\e[38;5;208mSensor\e[0m";
static xQueueHandle gpio_evt_queue = NULL;

static SemaphoreHandle_t motion_status_semaphore = NULL;

static bool is_presence_detected = false;

// Define your custom log function
int my_log_putchar(int ch)
{
    putchar((char)ch);
    return ch;
}

static void gpio_data_line_isr_handler(void *arg)
{
    bool pos_edge_detected = true;

    // Send the count to the task
    xQueueSendFromISR(gpio_evt_queue, &pos_edge_detected, NULL);
}

static bool hc_sr501_is_presence_detected(void)
{
    bool tmp = false;

    if(xSemaphoreTake(motion_status_semaphore, portMAX_DELAY) == pdTRUE)
    {
        tmp = is_presence_detected;
        xSemaphoreGive(motion_status_semaphore);
    }

    return tmp;
}

static void hc_sr501_task(void *pvParameters)
{
    gpio_config_t io_conf;
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);
    gpio_set_level(GPIO_DEBUG_LINE, 0);

    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    io_conf.intr_type = GPIO_INTR_POSEDGE;
    gpio_config(&io_conf);

    //install gpio isr service
    gpio_install_isr_service(0);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_SENSOR_DATA_LINE, gpio_data_line_isr_handler, NULL);

    //create a queue to handle gpio event from isr
    gpio_evt_queue = xQueueCreate(100, sizeof(bool));

    motion_status_semaphore = xSemaphoreCreateMutex();

    printf("\e[?25l\r\nStarting: \e[92m");
    fflush(stdout);
    for (int i = 0; i < 10; i++)
    {
        printf("█");
        fflush(stdout);
        vTaskDelay(1000 / portTICK_RATE_MS);
    }
    printf("\r\n\r\n%s: HC-SR501 Sensor Task Started\r\n", TAG);

    TickType_t no_detection_timeout = portMAX_DELAY;

    while(1)
    {
        bool status = false;

        if(pdTRUE == xQueueReceive(gpio_evt_queue, &status, no_detection_timeout))
        {
            if (portMAX_DELAY == no_detection_timeout)
            {
                printf("\r\n\e[1;92mMotion Detected\e[0m\r\n");

                if(xSemaphoreTake(motion_status_semaphore, portMAX_DELAY) == pdTRUE)
                {
                    is_presence_detected = true;
                    xSemaphoreGive(motion_status_semaphore);
                }

                no_detection_timeout = 6000 / portTICK_RATE_MS;
            }
        }
        else
        {
            printf("\r\n\e[1;91mMotion UN-Detected\e[0m\r\n");

            if(xSemaphoreTake(motion_status_semaphore, portMAX_DELAY) == pdTRUE)
            {
                is_presence_detected = false;
                xSemaphoreGive(motion_status_semaphore);
            }

            no_detection_timeout = portMAX_DELAY;
        }
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
        destAddr.sin_addr.s_addr = inet_addr(CONFIG_MOTION_IPV4_ADDR);
        destAddr.sin_family = AF_INET;
        destAddr.sin_port = htons(CONFIG_MOTION_SENSOR_PORT);
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
               CONFIG_MOTION_IPV4_ADDR,
               CONFIG_MOTION_SENSOR_PORT);

        while (1)
        {
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
                printf("\e[2KCommand Received from \e[93m%s\e[0m: \e[1;96m%s\e[0m", addr_str, rx_buffer);

                if (strcmp("READ\n", rx_buffer) == 0)
                {
                    bool is_detected = hc_sr501_is_presence_detected();

                    char response[64] = {0};

                    sprintf(response,
                            "{\"BedRoom\":{\"MotionSensor\":{\"Motion\": \"%s\"}}}\n",
                            is_detected? "true" : "false");

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
                    printf("\e[2KCommand \e[93m%s\e[0m NOT Supported\r\n\e[2K", rx_buffer);
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

    wifi_config_t wifi_config = {.sta = {.ssid = CONFIG_MOTION_WIFI_SSID,
                                         .password = CONFIG_MOTION_WIFI_PASSWORD}, };

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
                 CONFIG_MOTION_WIFI_SSID,
                 CONFIG_MOTION_WIFI_PASSWORD);

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
    xTaskCreate(hc_sr501_task,   "HC-SR501 Driver", 4096, NULL, 6, NULL);
    xTaskCreate(tcp_client_task, "TCP Client",   4096, NULL, 5, NULL);
}
