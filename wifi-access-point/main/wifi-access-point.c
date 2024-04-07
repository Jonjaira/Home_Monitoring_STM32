/*  WiFi softAP Example

 This example code is in the Public Domain (or CC0 licensed, at your option.)

 Unless required by applicable law or agreed to in writing, this
 software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 CONDITIONS OF ANY KIND, either express or implied.
 */
#include <stdio.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/socket.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#include "esp_vfs.h"
#include "esp_vfs_dev.h"
#include "driver/uart.h"

#define BUFFER_SIZE 1024

typedef struct thread_args
{
    TaskHandle_t thread_id;
    QueueHandle_t client_queue;
    int client_socket_fd;
    struct sockaddr_in address;
} thread_args_t;

// Node structure for the linked list of client handler threads
typedef struct client_handler_node
{
    thread_args_t args;
    struct client_handler_node *next;
} client_handler_node_t;

typedef struct uart_message
{

}uart_message_t;

static const char *TAG = "\e[38;5;208mWiFi Access Point\e[0m";
static QueueHandle_t uart_queue;

static client_handler_node_t *head = NULL;

static SemaphoreHandle_t uart_write_mutex;

client_handler_node_t* create_client_handler_node(struct sockaddr_in *address, int socket)
{
    client_handler_node_t *new_node = (client_handler_node_t*)malloc(sizeof(client_handler_node_t));

    if (new_node == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for client client \e[93m%s:%d\e[0m handler node: errno %d",
                 inet_ntoa(address->sin_addr),
                 ntohs(address->sin_port),
                 errno);

        return NULL;
    }

    new_node->args.client_queue = xQueueCreate(10, sizeof(char *));
    if (NULL == new_node->args.client_queue)
    {
        ESP_LOGE(TAG, "Failed to create a queue for client \e[93m%s:%d\e[0m",
                 inet_ntoa(address->sin_addr),
                 ntohs(address->sin_port));
        free(new_node);
        return NULL;
    }

    new_node->args.address = *address;
    new_node->args.client_socket_fd = socket;

    new_node->next = NULL;

    return new_node;
}

/*#######################################################################################################
               ____   _   _                  _       ____                   _             _
              / ___| | | (_)   ___   _ __   | |_    / ___|    ___     ___  | | __   ___  | |_
             | |     | | | |  / _ \ | '_ \  | __|   \___ \   / _ \   / __| | |/ /  / _ \ | __|
             | |___  | | | | |  __/ | | | | | |_     ___) | | (_) | | (__  |   <  |  __/ | |_
              \____| |_| |_|  \___| |_| |_|  \__|   |____/   \___/   \___| |_|\_\  \___|  \__|

#########################################################################################################*/
static void client_socket_handler_task(void *arg)
{
    struct thread_args args = *((struct thread_args*)arg);
    char buffer[BUFFER_SIZE] = {0};

    {
        TaskHandle_t xCurrentTaskHandle = xTaskGetCurrentTaskHandle();
        char* pcTaskName = pcTaskGetName(xCurrentTaskHandle);

        ESP_LOGI(TAG, "Task \e[96m%s \e[1;92mR E A D Y\e[0m", pcTaskName);
    }

    struct pollfd client_hdlr_poll_fds[] =
    {
        {.fd = args.client_socket_fd, .events = (POLLIN | POLLHUP | POLLERR | POLLNVAL) }
    };

    // Receive and echo data back to client
    while (1)
    {
        if (poll(client_hdlr_poll_fds, sizeof(client_hdlr_poll_fds) / sizeof(client_hdlr_poll_fds[0]), -1) == -1)
        {
            ESP_LOGE(TAG, "poll(L%d) failed. Suspending the task: errno %d", __LINE__ - 2, errno);
            vTaskSuspend(NULL);
        }

        memset(buffer, 0, sizeof(buffer));

        if (client_hdlr_poll_fds[0].revents & POLLIN)
        {
            int readbytes;

            /* ___              _            ___                     ___          __
              / _ \___ _______ (_)  _____   / _/______  __ _    ____/ (_)__ ___  / /_
             / , _/ -_) __/ -_) / |/ / -_) / _/ __/ _ \/  ' \  / __/ / / -_) _ \/ __/
            /_/|_|\__/\__/\__/_/|___/\__/ /_//_/  \___/_/_/_/  \__/_/_/\__/_//_/\__/  */
            if ( (readbytes = read (args.client_socket_fd, buffer, BUFFER_SIZE)) == -1)
            {
                if (errno == EBADF)
                {
                    ESP_LOGE(TAG, "\e[38;5;1mSocket Read error: Socket has been closed\e[0m: errno %d", errno);
                }
                else
                {
                    ESP_LOGE(TAG, "\e[38;5;1mSocket Read Error (errno = %d)\e[0m\n", errno);
                }
            }

            /* Detecting loss of connection from the server.
             * Was the poll triggered by the client socket? */
            /* Is read bytes empty? */
            if (!readbytes)
            {
                ESP_LOGE(TAG, "\n\e[38;5;1mLost connection to the Client!\e[0m");
                break;
            }

            ESP_LOGI(TAG, "Received \e[93m%s\e[0m from Client \e[92m%s:%d\e[0m\n",
                                     buffer,
                                     inet_ntoa(args.address.sin_addr),
                                     ntohs(args.address.sin_port));

            /* ____            __  __         __  _____   ___  ______
              / __/__ ___  ___/ / / /____    / / / / _ | / _ \/_  __/
             _\ \/ -_) _ \/ _  / / __/ _ \  / /_/ / __ |/ , _/ / /
            /___/\__/_//_/\_,_/  \__/\___/  \____/_/ |_/_/|_| /_/  */
            if (xSemaphoreTake(uart_write_mutex, portMAX_DELAY) == pdTRUE)
            {
                ESP_LOGI(TAG, "Sending \e[93m%s\e[0m from Client \e[92m%s:%d\e[0m to UART\n",
                         buffer,
                         inet_ntoa(args.address.sin_addr),
                         ntohs(args.address.sin_port));

                uart_write_bytes(UART_NUM_0, (const char*)buffer, readbytes);

                xSemaphoreGive(uart_write_mutex);
            }
        }

        if (client_hdlr_poll_fds[0].revents & (POLLHUP | POLLERR | POLLNVAL))
        {
            ESP_LOGE(TAG, "\n\e[38;5;1mLost connection to the Client!\e[0m");
            break;
        }
    }

    ESP_LOGI(TAG, "\n\e[38;5;208mClosing connection\e[0m from \e[92m%s:%d\e[0m",
             inet_ntoa(args.address.sin_addr),
             ntohs(args.address.sin_port));

    shutdown (args.client_socket_fd, SHUT_RDWR);
    close (args.client_socket_fd);

    vTaskDelete(NULL);
}

/*############################################################################################################################
                  ____                                          ____                   _             _
                 / ___|    ___   _ __  __   __   ___   _ __    / ___|    ___     ___  | | __   ___  | |_
                 \___ \   / _ \ | '__| \ \ / /  / _ \ | '__|   \___ \   / _ \   / __| | |/ /  / _ \ | __|
                  ___) | |  __/ | |     \ V /  |  __/ | |       ___) | | (_) | | (__  |   <  |  __/ | |_
                 |____/   \___| |_|      \_/    \___| |_|      |____/   \___/   \___| |_|\_\  \___|  \__|

#############################################################################################################################*/
static void server_socket_handler_task(void *arg)
{
    char addr_str[128];
    int server_socket;
    struct sockaddr_in address;
    uint addrlen = sizeof(address);
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_family = AF_INET;
    address.sin_port = htons(CONFIG_ESP_WIFI_AP_PORT);
    inet_ntoa_r(address.sin_addr, addr_str, sizeof(addr_str) - 1);

    int addr_family = AF_INET;
    int ip_protocol = IPPROTO_IP;
    if ((server_socket = socket(addr_family, SOCK_STREAM, ip_protocol)) < 0)
    {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        vTaskSuspend(NULL);
    }
    ESP_LOGI(TAG, "TCPIP Server Socket created");

    if (bind(server_socket, (struct sockaddr*)&address, sizeof(address)))
    {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        vTaskSuspend(NULL);
    }
    ESP_LOGI(TAG, "TCPIP Server Socket binded");

    if (listen(server_socket, 10))
    {
        ESP_LOGE(TAG, "Error occured during listen: errno %d", errno);
        vTaskSuspend(NULL);
    }
    ESP_LOGI(TAG, "TCPIP Server Listening on port \e[93m%d\e[0m...",
             CONFIG_ESP_WIFI_AP_PORT);

    struct pollfd main_poll_fds[] = {
        {.fd = server_socket, .events = POLLIN}
    };

    while (1)
    {
        if (poll(main_poll_fds, sizeof(main_poll_fds) / sizeof(main_poll_fds[0]), -1) < 0)
        {
            ESP_LOGE(TAG, "poll(%d): errno %d", __LINE__, errno);
            continue;
        }

        if (main_poll_fds[0].revents & POLLIN)
        {
            int client_socket;

            /* ___                   __    _                     _                 ___          __                               __  _
              / _ |___________ ___  / /_  (_)__  _______  __ _  (_)__  ___ _  ____/ (_)__ ___  / /_  _______  ___  ___  ___ ____/ /_(_)__  ___
             / __ / __/ __/ -_) _ \/ __/ / / _ \/ __/ _ \/  ' \/ / _ \/ _ `/ / __/ / / -_) _ \/ __/ / __/ _ \/ _ \/ _ \/ -_) __/ __/ / _ \/ _ \
            /_/ |_\__/\__/\__/ .__/\__/ /_/_//_/\__/\___/_/_/_/_/_//_/\_, /  \__/_/_/\__/_//_/\__/  \__/\___/_//_/_//_/\__/\__/\__/_/\___/_//_/
                            /_/                                      /___/                                                                     */
            if ((client_socket = accept(server_socket, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0)
            {
                ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
                continue;
            }

            /* Before creating a new node and a new task, we see if there is an
             * existing node for the connected device. This will happen if the
             * client suddenly loses power without first execute a proper network
             * leave */
            bool need_new_node = true;
            for (client_handler_node_t *current = head;
                 current != NULL;
                 current = current->next)
            {
                if (((uint32_t)current->args.address.sin_addr.s_addr) == ((uint32_t)address.sin_addr.s_addr))
                {
                    /* There is a node for this client. We then proceed to delete the existing
                     * stale taks and create a new one */
                    ESP_LOGW(TAG, "\e[93mThere is a node and a taks already for client \e[1;96m%s:%d\e[0m",
                             inet_ntoa(address.sin_addr),
                             ntohs(address.sin_port));

                    /* In this case, only the socket needs to be refreshed */
                    current->args.client_socket_fd = client_socket;

                    need_new_node = false;
                    break;
                }
            }

            if (need_new_node)
            {
                /*_____             __                     __                   __  __           __     ___               ___          __
                 / ___/______ ___ _/ /____   ___  ___  ___/ /__   ___ ____  ___/ / / /____ ____ / /__  / _/__  ____  ____/ (_)__ ___  / /_
                / /__/ __/ -_) _ `/ __/ -_) / _ \/ _ \/ _  / -_) / _ `/ _ \/ _  / / __/ _ `(_-</  '_/ / _/ _ \/ __/ / __/ / / -_) _ \/ __/
                \___/_/  \__/\_,_/\__/\__/ /_//_/\___/\_,_/\__/  \_,_/_//_/\_,_/  \__/\_,_/___/_/\_\ /_/ \___/_/    \__/_/_/\__/_//_/\__/ */
                client_handler_node_t *new_node = create_client_handler_node(&address, client_socket);

                if (NULL == new_node)
                {
                    ESP_LOGE(TAG, "\e[91mUnable to create a node for the connected client\e[0m");

                    close(client_socket);

                    continue;
                }

                // Add the new node to the front of the linked list
                new_node->next = head;
                head = new_node;

                ESP_LOGI(TAG, "Creating Task for \e[93m%s:%d\e[0m",
                         inet_ntoa(new_node->args.address.sin_addr),
                         ntohs(new_node->args.address.sin_port));

                char task_name[32] = {0};
                sprintf(task_name, "Client %s Handler", inet_ntoa(new_node->args.address.sin_addr));

                xTaskCreate(client_socket_handler_task,
                            task_name,
                            4096,
                            (void*)&new_node->args,
                            4,
                            &new_node->args.thread_id);

                uint8_t node_count = 0;
                for (client_handler_node_t *current = head;
                     current != NULL;
                     current = current->next,
                     node_count++);

                ESP_LOGI(TAG, "Total nodes connected = \e[96m%d\e[0m", node_count);
            }
        }
    }

    vTaskDelete(NULL);
}

/*#########################################################################
                  _   _      _      ____    _____
                 | | | |    / \    |  _ \  |_   _|
                 | | | |   / _ \   | |_) |   | |
                 | |_| |  / ___ \  |  _ <    | |
                  \___/  /_/   \_\ |_| \_\   |_|

##########################################################################*/
static void uart_handler_task(void *arg)
{
    // Configure parameters of an UART driver,
    // communication pins and install the driver
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(UART_NUM_0, &uart_config);

    // Install UART driver, and get the queue.
    uart_driver_install(UART_NUM_0, BUFFER_SIZE * 2, BUFFER_SIZE * 2, 100, &uart_queue, 0);

    uart_write_mutex = xSemaphoreCreateMutex();

    uart_event_t event;
    uint8_t buffer[BUFFER_SIZE] = {0};
    size_t buffer_index = 0;

    vTaskDelay(10000 / portTICK_RATE_MS);

    uart_write_bytes(UART_NUM_0, "UART Initialized\n", strlen("UART Initialized\n"));

    while(1)
    {
        // Waiting for UART event.
        if (xQueueReceive(uart_queue, (void*)&event, (portTickType)portMAX_DELAY))
        {
            memset(buffer, 0, BUFFER_SIZE);

            switch (event.type)
            {
                /* ___              _            ___                 __  _____   ___  ______
                  / _ \___ _______ (_)  _____   / _/______  __ _    / / / / _ | / _ \/_  __/
                 / , _/ -_) __/ -_) / |/ / -_) / _/ __/ _ \/  ' \  / /_/ / __ |/ , _/ / /
                /_/|_|\__/\__/\__/_/|___/\__/ /_//_/  \___/_/_/_/  \____/_/ |_/_/|_| /_/   */

                // Event of UART receving data
                // We'd better handler data event fast, there would be much more data events than
                // other types of events. If we take too much time on data event, the queue might be full.
                case UART_DATA:
                    uart_read_bytes(UART_NUM_0, &buffer[buffer_index], event.size, portMAX_DELAY);
                    buffer_index += event.size;

                    /* Check if the \n character has been received */
                    if (buffer[buffer_index - 1] == '\n')
                    {
                        buffer[buffer_index] = 0;
                        ESP_LOGI(TAG, "Command Received from UART: \e[93m%s\e[0m", buffer);

                        /* ____            __  __         ___   __   __         ___          __
                          / __/__ ___  ___/ / / /____    / _ | / /  / /    ____/ (_)__ ___  / /____
                         _\ \/ -_) _ \/ _  / / __/ _ \  / __ |/ /__/ /__  / __/ / / -_) _ \/ __(_-<
                        /___/\__/_//_/\_,_/  \__/\___/ /_/ |_/____/____/  \__/_/_/\__/_//_/\__/___/ */
                        for (client_handler_node_t *current = head;
                             current != NULL;
                             current = current->next)
                        {
                            ESP_LOGI(TAG, "Sending \e[93m%s\e[0m To Client \e[92m%s:%d\e[0m\n",
                                     buffer,
                                     inet_ntoa(current->args.address.sin_addr),
                                     ntohs(current->args.address.sin_port));

                            /* Write all data out */
                            if (write (current->args.client_socket_fd, buffer, buffer_index) == -1)
                            {
                                ESP_LOGE(TAG, "client socket write error: errno %d", errno);
                                break;
                            }

                            /* To make processing easier, we wait 200 ms before sending to the
                             * next client. This will allow enough time for the command to reach
                             * the client and the client responding. This will "serialize" the
                             * clients responses and make synchronization easier. */
                            vTaskDelay(200 / portTICK_RATE_MS);
                        }

                        buffer_index = 0;
                    }

                    break;
//##############################################################################################################
                    // Event of HW FIFO overflow detected
                case UART_FIFO_OVF:
                    ESP_LOGI(TAG, "hw fifo overflow");
                    // If fifo overflow happened, you should consider adding flow control for your application.
                    // The ISR has already reset the rx FIFO,
                    // As an example, we directly flush the rx buffer here in order to read more data.
                    uart_flush_input(UART_NUM_0);
                    xQueueReset(uart_queue);
                    break;

                    // Event of UART ring buffer full
                case UART_BUFFER_FULL:
                    ESP_LOGI(TAG, "ring buffer full");
                    // If buffer full happened, you should consider encreasing your buffer size
                    // As an example, we directly flush the rx buffer here in order to read more data.
                    uart_flush_input(UART_NUM_0);
                    xQueueReset(uart_queue);
                    break;

                case UART_PARITY_ERR:
                    ESP_LOGI(TAG, "uart parity error");
                    break;

                    // Event of UART frame error
                case UART_FRAME_ERR:
                    ESP_LOGI(TAG, "uart frame error");
                    break;

                    /* We will be reusing this event to send custom events from other tasks */
                case UART_EVENT_MAX:

                    break;
                    // Others
                default:
                    ESP_LOGI(TAG, "uart event type: %d", event.type);
                    break;
            }
        }
    }

    vTaskDelete(NULL);
}

static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED)
    {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t*)event_data;
        ESP_LOGI(TAG, "station \e[92m"MACSTR"\e[0m J O I N, AID=%d", MAC2STR(event->mac), event->aid);
    }
    else if (event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t*)event_data;
        ESP_LOGI(TAG, "station \e[92m"MACSTR"\e[0m L E A V E, AID=%d", MAC2STR(event->mac), event->aid);
    }
}

static void wifi_init_softap()
{
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));

    wifi_config_t wifi_config = {.ap = {.ssid = CONFIG_ESP_WIFI_AP_SSID,
        .ssid_len = strlen(CONFIG_ESP_WIFI_AP_SSID), .password = CONFIG_ESP_WIFI_AP_PASSWORD,
        .max_connection = CONFIG_ESP_MAX_STA_CONN, .authmode = WIFI_AUTH_WPA_WPA2_PSK}, };
    if (strlen(CONFIG_ESP_WIFI_AP_PASSWORD) == 0)
    {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG,
             "wifi_init_softap finished. SSID:\e[93m%s\e[0m password:\e[93m%s\e[0m",
             CONFIG_ESP_WIFI_AP_SSID,
             CONFIG_ESP_WIFI_AP_PASSWORD);
}

void app_main()
{
    ESP_ERROR_CHECK(nvs_flash_init());

    ESP_LOGI(TAG, "\e[93mESP8266 WiFi Access Point\e[0m");
    wifi_init_softap();

    xTaskCreate(server_socket_handler_task, "Server Socket",   4096, NULL, 5, NULL);
    xTaskCreate(uart_handler_task,          "UART Handler",   4096, NULL, 6, NULL);
}
