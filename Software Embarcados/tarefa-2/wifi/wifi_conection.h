#ifndef THING_SPEAK_H
#define THING_SPEAK_H

#include "wifi_parametros.h"
#include "pico/cyw43_arch.h"
#include "lwip/tcp.h"
#include "../inc/ssd1306.h"
#include "hardware/i2c.h"

extern uint8_t *ip_address;

int conectar_wifi(uint8_t width, uint8_t n_pages, int buffer_length);
bool is_wifi_connected();

#endif