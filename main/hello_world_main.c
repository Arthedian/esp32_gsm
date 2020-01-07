/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <tcpip_adapter.h>
#include <gsm.h>
#include <esp_log.h>
#include <driver/gpio.h>
#include <lwip/netdb.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"

#define GSM_UART_NUM UART_NUM_0
#define GSM_TX GPIO_NUM_3
#define GSM_RX GPIO_NUM_1
#define GSM_RST GPIO_NUM_5

#define TAG "main"

#define WEB_SERVER "www.example.com"
#define WEB_PORT "80"
#define WEB_PATH "/"


static const char *REQUEST = "GET " WEB_PATH " HTTP/1.0\r\n"
"Host: "WEB_SERVER":"WEB_PORT"\r\n"
"User-Agent: esp-idf/1.0 esp32\r\n"
"\r\n";


void http_get() {

    const struct addrinfo hints = {
            .ai_family = AF_INET,
            .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *res;
    struct in_addr *addr;
    int s, r;
    char recv_buf[64];

    for (int j = 0; j < 2; ++j) {

        int err = getaddrinfo(WEB_SERVER, WEB_PORT, &hints, &res);

        if (err != 0 || res == NULL) {
            ESP_LOGE(TAG, "DNS lookup failed err=%d res=%p", err, res);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }

        /* Code to print the resolved IP.
           Note: inet_ntoa is non-reentrant, look at ipaddr_ntoa_r for "real" code */
        addr = &((struct sockaddr_in *) res->ai_addr)->sin_addr;
        ESP_LOGI(TAG, "DNS lookup succeeded. IP=%s", inet_ntoa(*addr));

        s = socket(res->ai_family, res->ai_socktype, 0);
        if (s < 0) {
            ESP_LOGE(TAG, "... Failed to allocate socket.");
            freeaddrinfo(res);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "... allocated socket");

        if (connect(s, res->ai_addr, res->ai_addrlen) != 0) {
            ESP_LOGE(TAG, "... socket connect failed errno=%d", errno);
            close(s);
            freeaddrinfo(res);
            vTaskDelay(4000 / portTICK_PERIOD_MS);
            continue;
        }

        ESP_LOGI(TAG, "... connected");
        freeaddrinfo(res);

        if (write(s, REQUEST, strlen(REQUEST)) < 0) {
            ESP_LOGE(TAG, "... socket send failed");
            close(s);
            vTaskDelay(4000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "... socket send success");

        struct timeval receiving_timeout;
        receiving_timeout.tv_sec = 5;
        receiving_timeout.tv_usec = 0;
        if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &receiving_timeout,
                       sizeof(receiving_timeout)) < 0) {
            ESP_LOGE(TAG, "... failed to set socket receiving timeout");
            close(s);
            vTaskDelay(4000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "... set socket receiving timeout success");

        /* Read HTTP response */
        do {
            bzero(recv_buf, sizeof(recv_buf));
            r = read(s, recv_buf, sizeof(recv_buf) - 1);
            for (int i = 0; i < r; i++) {
                putchar(recv_buf[i]);
            }
        } while (r > 0);

        ESP_LOGI(TAG, "... done reading from socket. Last read return=%d errno=%d.", r, errno);
        close(s);
    }
}

void app_main()
{
    tcpip_adapter_init();
    for (int i = 0; i < 10; ++i) {
        ESP_LOGE(TAG, "free memory %d\n",esp_get_free_heap_size() );
        gsm_init(GSM_UART_NUM, GSM_TX, GSM_RX, GSM_RST);
        gsm_start();
        for (int i = 0; i <30; ++i) {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
        http_get();
        gsm_stop();
        gsm_deinit();
        ESP_LOGE(TAG, "Done deinit and waiting for 10 seconds");
        for (int i = 0; i <20; ++i) {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }
}
