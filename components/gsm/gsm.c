//
// Created by arthedian on 10.11.19.
//

#include <driver/gpio.h>
#include "gsm.h"
#include "lib/esp_modem.h"
#include "esp_log.h"
#include "lib/sim800.h"

#define TAG "GSM"

#include <string.h>

typedef struct gsm_handlers_t {
    void (*fun_ptr)(void *);

    enum gsm_event_type event_type;
    struct gsm_handlers_t *next;
} gsm_handlers_t;

modem_dte_t *gsm_dte;
modem_dce_t *gsm_dce;
uart_port_t gsm_uart_num;
volatile int gsm_status = 0;
char imei[16 + 1] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

gsm_handlers_t *gsm_handlers_list_begin = NULL;
gsm_handlers_t *gsm_handlers_list_end = NULL;
SemaphoreHandle_t gsmHandlersSemaphore;

void gsm_handlers_take_semaphore() {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    while (xSemaphoreTakeFromISR(gsmHandlersSemaphore, &xHigherPriorityTaskWoken) != pdTRUE) {
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void gsm_handlers_give_semaphore() {
    xSemaphoreGive(gsmHandlersSemaphore);
}


void gsm_event_happend(void *data) {
    enum gsm_event_type event_type = (enum gsm_event_type) data;
    ESP_LOGD(TAG, "Event number %d happend", event_type);
    gsm_handlers_take_semaphore();
    gsm_handlers_t *gsm_handlers_list = gsm_handlers_list_begin;
    while (gsm_handlers_list) {
        if (event_type == gsm_handlers_list->event_type) gsm_handlers_list->fun_ptr(NULL);
        gsm_handlers_list = gsm_handlers_list->next;
    }
    gsm_handlers_give_semaphore();
    ESP_LOGD(TAG, "gsm event happend task finished");
    vTaskDelete(NULL);
}

static void
gsm_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    switch (event_id) {
        case MODEM_EVENT_PPP_START:
            ESP_LOGI(TAG, "Modem PPP Started");
            gsm_status = 1;
            break;
        case MODEM_EVENT_PPP_CONNECT:
            gsm_status = 2;
            ESP_LOGI(TAG, "Modem Connect to PPP Server");
            ppp_client_ip_info_t *ipinfo = (ppp_client_ip_info_t *) (event_data);
            ESP_LOGI(TAG, "~~~~~~~~~~~~~~");
            ESP_LOGI(TAG, "IP          : "
                    IPSTR, IP2STR(&ipinfo->ip));
            ESP_LOGI(TAG, "Netmask     : "
                    IPSTR, IP2STR(&ipinfo->netmask));
            ESP_LOGI(TAG, "Gateway     : "
                    IPSTR, IP2STR(&ipinfo->gw));
            ESP_LOGI(TAG, "Name Server1: "
                    IPSTR, IP2STR(&ipinfo->ns1));
            ESP_LOGI(TAG, "Name Server2: "
                    IPSTR, IP2STR(&ipinfo->ns2));
            ESP_LOGI(TAG, "~~~~~~~~~~~~~~");
            //xEventGroupSetBits(event_group, CONNECT_BIT);
            xTaskCreate(&gsm_event_happend, "gsm_event_happend", 1024 * 2, (void *) GSM_CONNECTED, 5, NULL);
            break;
        case MODEM_EVENT_PPP_DISCONNECT:
            gsm_status = 3;
            ESP_LOGI(TAG, "Modem Disconnect from PPP Server");
            xTaskCreate(gsm_event_happend, "gsm_event_happend", 1024 * 2, (void *) GSM_DISCONNECTED, 5, NULL);
            //if (gsm_should_be_connected) gsm_start();
            break;
        case MODEM_EVENT_PPP_STOP:
            gsm_status = 0;
            ESP_LOGI(TAG, "Modem PPP Stopped");
            //xEventGroupSetBits(event_group, STOP_BIT);
            break;
        case MODEM_EVENT_UNKNOWN:
            gsm_status = 4;
            ESP_LOGW(TAG, "Unknow line received: %s", (char *) event_data);
            break;
        default:
            break;
    }
}

void gsm_init(uart_port_t uart_num, int tx_pin, int rx_pin, int rst_pin) {
    ESP_LOGI(TAG, "Start initializing.");

    gsmHandlersSemaphore = xSemaphoreCreateBinary();
    if (gsmHandlersSemaphore == NULL) {
        esp_restart();
    }
    xSemaphoreGive(gsmHandlersSemaphore);

    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;

    io_conf.pin_bit_mask = (1ULL << rst_pin);

    gpio_config(&io_conf);

    //Just to be sure, restart the modem before connecting
    gpio_set_level(rst_pin, 0);
    ESP_LOGD(TAG, "Resseting");
    vTaskDelay(5000 / portTICK_RATE_MS);
    gpio_set_level(rst_pin, 1);
    ESP_LOGD(TAG, "DOne resseting");
    vTaskDelay(15000 / portTICK_RATE_MS);

    /*uart_config_t uart_config = {
            .baud_rate = 9600,
            .data_bits = UART_DATA_8_BITS,
            .parity    = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    ESP_LOGD(TAG, "gsm test 0");
    vTaskDelay(1000 / portTICK_RATE_MS);
    if (uart_param_config(uart_num, &uart_config)!= ESP_OK) return;
    if (uart_set_pin(uart_num, tx_pin, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE)!= ESP_OK) return;
    if (uart_driver_install(uart_num, 1024 * 2, 0, 0, NULL, 0)!= ESP_OK) return;

    printf("gsm test 1\n");
    uart_write_bytes(uart_num, "AT\r\n", strlen("AT\r\n"));
    printf("gsm test 2\n");
    uint8_t dataToRead[512];
    int len = uart_read_bytes(uart_num, dataToRead, 512, 1000 / portTICK_RATE_MS);
    printf("gsm test 3\n");
    uart_driver_delete(uart_num);
    printf("gsm test 4\n");
    vTaskDelay(1000 / portTICK_RATE_MS);

    for (int i = 0; i < len; ++i) {
        printf("%02X", dataToRead[i]);
    }
    printf("\n");*/


    gsm_uart_num = uart_num;
    esp_modem_dte_config_t config = ESP_MODEM_DTE_DEFAULT_CONFIG(uart_num);
    gsm_dte = esp_modem_dte_init(&config, tx_pin, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    /* Register event handler */
    ESP_ERROR_CHECK(esp_modem_add_event_handler(gsm_dte, gsm_event_handler, NULL));
    /* create dce object */
    gsm_dce = sim800_init(gsm_dte);

    ESP_ERROR_CHECK(gsm_dce->set_flow_ctrl(gsm_dce, MODEM_FLOW_CONTROL_NONE));
    ESP_ERROR_CHECK(gsm_dce->store_profile(gsm_dce));

    ESP_LOGD(TAG, "Module: %s", gsm_dce->name);
    ESP_LOGD(TAG, "Operator: %s", gsm_dce->oper);
    ESP_LOGD(TAG, "IMEI: %s", gsm_dce->imei);
    memcpy(imei, gsm_dce->imei, strlen(gsm_dce->imei));
    ESP_LOGD(TAG, "IMSI: %s", gsm_dce->imsi);
    ESP_LOGI(TAG, "Finished initializing.");

}

void gsm_register_handler(void (*fun_ptr)(void *), enum gsm_event_type event_type) {
    gsm_handlers_t *new_handler = malloc(sizeof(gsm_handlers_t));
    new_handler->fun_ptr = fun_ptr;
    new_handler->event_type = event_type;
    new_handler->next = NULL;
    gsm_handlers_take_semaphore();
    if (gsm_handlers_list_end == NULL) {
        gsm_handlers_list_begin = new_handler;
        gsm_handlers_list_end = new_handler;
    } else {
        gsm_handlers_list_end->next = new_handler;
        gsm_handlers_list_end = new_handler;
    }
    gsm_handlers_give_semaphore();
}

void gsm_deinit(){
    if (gsm_dce) ESP_ERROR_CHECK(gsm_dce->deinit(gsm_dce));
    if (gsm_dte) ESP_ERROR_CHECK(gsm_dte->deinit(gsm_dte));
}

void gsm_start() {
    /* Setup PPP environment */
    /* Print Module ID, Operator, IMEI, IMSI */
    ESP_LOGD(TAG, "Module: %s", gsm_dce->name);
    ESP_LOGD(TAG, "Operator: %s", gsm_dce->oper);
    ESP_LOGD(TAG, "IMEI: %s", gsm_dce->imei);
    ESP_LOGD(TAG, "IMSI: %s", gsm_dce->imsi);
    //wait until it is connected to the network
    uint32_t rssi = 0, ber = 0;
    int counter = 0;
    while (rssi == 0 && (counter < 20)) { //waiting until I am connected to the operator
        //ESP_ERROR_CHECK(gsm_dce->get_signal_quality(gsm_dce, &rssi, &ber));
        gsm_dce->get_signal_quality(gsm_dce, &rssi, &ber);
        ESP_LOGI(TAG, "rssi: %d, ber: %d", rssi, ber);
        counter++;
        vTaskDelay(1000 / portTICK_RATE_MS);
    }
    /* Get battery voltage */
    uint32_t voltage = 0, bcs = 0, bcl = 0;
    ESP_ERROR_CHECK(gsm_dce->get_battery_status(gsm_dce, &bcs, &bcl, &voltage));
    ESP_LOGI(TAG, "Battery voltage: %d mV", voltage);
    esp_err_t ret = esp_modem_setup_ppp(gsm_dte);
    ESP_LOGE(TAG, "ret %d, should be ok %d, error %d", ret, ESP_OK, ESP_FAIL);
}

void gsm_start_ppp(){
    esp_err_t ret = esp_modem_setup_ppp(gsm_dte);
    ESP_LOGE(TAG, "ret %d, should be ok %d, error %d", ret, ESP_OK, ESP_FAIL);
}

void gsm_stop() {
    /* Exit PPP mode */
    if (gsm_dte) ESP_LOGD(TAG, "ok");
    else
        ESP_LOGE(TAG, "DTE IS NONE");
    ESP_ERROR_CHECK(esp_modem_exit_ppp(gsm_dte));
    //gsm_should_be_connected = 0;
    /* Power down module */
    /*ESP_ERROR_CHECK(gsm_dce->power_down(gsm_dce));
    ESP_LOGI(TAG, "Power down");
    ESP_ERROR_CHECK(gsm_dce->deinit(gsm_dce));
    ESP_ERROR_CHECK(gsm_dte->deinit(gsm_dte));*/
    while (gsm_status != 0) {
        ESP_LOGD(TAG, "gsm status %d", gsm_status);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

/*void gsm_check_sms(){ //TODO fixed it
    ESP_LOGI(TAG, "GSM check SMS started");
    gsm_stop();
    while(gsm_status !=0){}
    ESP_LOGI(TAG, "GSM stopped");
    //TODO check SMS
    uart_write_bytes(gsm_uart_num, "AT\r\n", 4);
    uint8_t data[256];
    int len = uart_read_bytes(gsm_uart_num, data, 254, 1000 / portTICK_RATE_MS);
    data[len] = '\0';<
    printf("%d '%s'", len, data);
    //AT
    //AT+CMGF=1
    //AT+CMGL="REC UNREAD"
    gsm_start();
    while(gsm_status <1){}
    ESP_LOGD(TAG, "GSM started");
    //TODO wait until start;
}*/

int gsm_is_connected() {
    //ESP_LOGI(TAG, "connected should be 2, is %d", gsm_status);
    return (gsm_status == 2);
}

char *gsm_get_imei() {
    return imei;
}