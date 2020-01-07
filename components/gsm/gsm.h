//
// Created by arthedian on 10.11.19.
//

#ifndef REQUESTER_FIRMWARE_GSM_H
#define REQUESTER_FIRMWARE_GSM_H

#include <driver/uart.h>

//TODO comments

enum gsm_event_type{GSM_SMS_RECEIVED, GSM_CONNECTED, GSM_DISCONNECTED};

void gsm_init(uart_port_t uart_num, int tx_pin, int rx_pin, int rst_pin);

void gsm_register_handler(void (*fun_ptr)(void *) , enum gsm_event_type event_type);

void gsm_start();

void gsm_stop();

//void gsm_check_sms();

int gsm_is_connected();

char *gsm_get_imei();

void gsm_deinit();

void gsm_start_ppp();

#endif //REQUESTER_FIRMWARE_GSM_H
