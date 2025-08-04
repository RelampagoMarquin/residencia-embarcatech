#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include "pico/stdlib.h" // Adicionado para usar uint8_t

// =================================================================================
//                                INTERFACE PÚBLICA
// =================================================================================

/**
 * @brief Inicia a tarefa de MQTT.
 *
 * Esta função cria as filas necessárias e a tarefa que gerencia a conexão Wi-Fi
 * (usando sua biblioteca) e a publicação de mensagens MQTT.
 * Os parâmetros do Wi-Fi devem ser configurados no arquivo 'wifi_parametros.h'.
 * * @param width Largura do display, necessária para a função conectar_wifi.
 * @param n_pages Número de páginas do display, necessário para a função conectar_wifi.
 * @param buffer_length Tamanho do buffer do display, necessário para a função conectar_wifi.
 */
void mqtt_handler_start(uint8_t width, uint8_t n_pages, int buffer_length);


/**
 * @brief Envia uma medição de temperatura para a fila de publicação.
 *
 * @param temp O valor da temperatura (inteiro) a ser publicado.
 */
void mqtt_handler_publish_temperature(int temp);


/**
 * @brief Envia uma direção do joystick para a fila de publicação.
 *
 * @param direction A string da direção (ex: "cima", "baixo") a ser publicada.
 */
void mqtt_handler_publish_joystick(const char* direction);


#endif // MQTT_HANDLER_H