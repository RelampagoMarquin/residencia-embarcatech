#include "mqtt_helper.h"
#include <string.h>
#include <stdio.h>

// Função interna para codificar o campo "Remaining Length" do MQTT
static int encode_remaining_length(uint8_t *buf, size_t buf_len, uint32_t length) {
    int bytes_encoded = 0;
    do {
        if (bytes_encoded >= 4 || (size_t)bytes_encoded >= buf_len) return -1; // Comprimento máximo ou buffer estourado
        uint8_t d = length % 128;
        length /= 128;
        if (length > 0) {
            d |= 128;
        }
        buf[bytes_encoded++] = d;
    } while (length > 0);
    return bytes_encoded;
}

int mqtt_helper_build_connect(uint8_t *buffer, size_t buf_len, const char *client_id, uint16_t keep_alive_s) {
    if (!buffer || !client_id) return 0;

    // --- Cabeçalho Fixo ---
    buffer[0] = 0x10; // Tipo de pacote: CONNECT
    size_t pos = 1;

    // --- Cabeçalho Variável ---
    const uint8_t var_header[] = {
        0x00, 0x04, 'M', 'Q', 'T', 'T', // Nome do protocolo
        0x04,                          // Nível do protocolo (3.1.1)
        0x02,                          // Flags: Clean Session
        (uint8_t)(keep_alive_s >> 8),  // Keep-alive MSB
        (uint8_t)(keep_alive_s & 0xFF) // Keep-alive LSB
    };

    // --- Payload ---
    uint16_t client_id_len = strlen(client_id);
    uint32_t remaining_length = sizeof(var_header) + 2 + client_id_len;

    uint8_t len_buf[4];
    int len_bytes = encode_remaining_length(len_buf, sizeof(len_buf), remaining_length);
    if (len_bytes < 0) return 0;

    size_t total_len = 1 + len_bytes + remaining_length;
    if (total_len > buf_len) return 0; // Verifica se cabe no buffer

    memcpy(&buffer[pos], len_buf, len_bytes);
    pos += len_bytes;

    memcpy(&buffer[pos], var_header, sizeof(var_header));
    pos += sizeof(var_header);

    buffer[pos++] = (uint8_t)(client_id_len >> 8);
    buffer[pos++] = (uint8_t)(client_id_len & 0xFF);
    memcpy(&buffer[pos], client_id, client_id_len);
    
    return total_len;
}

int mqtt_helper_build_publish(uint8_t *buffer, size_t buf_len, const char *topic, const char *payload) {
    if (!buffer || !topic || !payload) return 0;

    // --- Cabeçalho Fixo ---
    buffer[0] = 0x30; // Tipo: PUBLISH, Flags: QoS=0
    size_t pos = 1;

    // --- Remaining Length ---
    uint16_t topic_len = strlen(topic);
    uint16_t payload_len = strlen(payload);
    uint32_t remaining_length = 2 + topic_len + payload_len;
    
    uint8_t len_buf[4];
    int len_bytes = encode_remaining_length(len_buf, sizeof(len_buf), remaining_length);
    if (len_bytes < 0) return 0;

    size_t total_len = 1 + len_bytes + remaining_length;
    if (total_len > buf_len) return 0;

    memcpy(&buffer[pos], len_buf, len_bytes);
    pos += len_bytes;

    buffer[pos++] = (uint8_t)(topic_len >> 8);
    buffer[pos++] = (uint8_t)(topic_len & 0xFF);
    memcpy(&buffer[pos], topic, topic_len);
    pos += topic_len;

    memcpy(&buffer[pos], payload, payload_len);

    return total_len;
}

