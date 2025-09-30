#ifndef MQTT_HELPER_H
#define MQTT_HELPER_H

#include <stdint.h>
#include <stddef.h>

/**
 * @brief Monta um pacote MQTT CONNECT.
 *
 * @param buffer O buffer de saída onde o pacote será escrito.
 * @param buf_len O tamanho total do buffer de saída.
 * @param client_id O ID do cliente a ser usado na conexão.
 * @param keep_alive_s O tempo de keep-alive em segundos.
 * @return O número de bytes escritos no buffer, ou 0 em caso de erro.
 */
int mqtt_helper_build_connect(uint8_t *buffer, size_t buf_len, const char *client_id, uint16_t keep_alive_s);

/**
 * @brief Monta um pacote MQTT PUBLISH.
 *
 * @param buffer O buffer de saída onde o pacote será escrito.
 * @param buf_len O tamanho total do buffer de saída.
 * @param topic O tópico no qual a mensagem será publicada.
 * @param payload A mensagem (carga útil) a ser enviada.
 * @return O número de bytes escritos no buffer, ou 0 em caso de erro.
 */
int mqtt_helper_build_publish(uint8_t *buffer, size_t buf_len, const char *topic, const char *payload);

#endif // MQTT_HELPER_H

