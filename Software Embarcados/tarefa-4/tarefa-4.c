#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "FreeRTOS.h"
#include "task.h"
#include "mqtt_psk_client.h"

// --- CONFIGURAÇÕES DA TAREFA ---
#define WIFI_SSID             "tarefa-mqtt"
#define WIFI_PASSWORD         "laica@2025"
#define MQTT_HOST             "192.168.0.15" // IP da sua máquina rodando o Mosquitto ou substituir pelo host
#define MQTT_PORT             8849
#define MQTT_CLIENT_ID        "aluno49"
#define MQTT_TOPIC_TEMP       "/aluno49/bitdoglab/temperatura"
#define MQTT_TOPIC_BUTTON     "/aluno49/bitdoglab/botao"
#define MQTT_TOPIC_SUBSCRIBE  "/aluno49/sub/#"
#define BUTTON_PIN            5
#define KEEP_ALIVE_SECONDS    60
#define MAIN_TASK_STACK_SIZE  (8192)
#define RX_TASK_STACK_SIZE    (4096)

// O Mosquitto lê a string hexadecimal "ABCD49EF1234" e a converte para estes bytes.
const unsigned char psk_identity[] = "aluno49";
const unsigned char psk_key[]      = { 0xAB, 0xCD, 0x49, 0xEF, 0x12, 0x34 };
const size_t psk_len               = sizeof(psk_key);
// ------------------------------------

// --- CONTEXTO GLOBAL E PROTÓTIPOS ---
static mqtt_client_context_t ctx;
static TaskHandle_t rx_task_handle = NULL;
int build_mqtt_connect(const char *client_id, unsigned char *buf, int buf_len);
int build_mqtt_publish(const char *topic, const char *payload, unsigned char *buf, int buf_len);
int build_mqtt_subscribe(const char *topic, unsigned char *buf, int buf_len);
int build_mqtt_pingreq(unsigned char *buf, int buf_len);
float read_mcu_temperature();

// --- TAREFA DEDICADA AO RECEBIMENTO DE MENSAGENS ---
static void rx_task(void *pvParameters) {
    unsigned char rx_buf[512];
    printf("[rx_task] Tarefa de recebimento iniciada.\n"); 
    while (1) {
        int r = mqtt_psk_client_recv(&ctx, rx_buf, sizeof(rx_buf));
        if (r > 0) {
            uint8_t packet_type = rx_buf[0] & 0xF0;
            switch (packet_type) {
                case 0x90: // SUBACK
                    printf("[rx] SUBACK recebido! Inscrição confirmada.\n");
                    break;
                case 0xD0: // PINGRESP
                    //printf("[rx] PINGRESP recebido.\n");
                    break;
                case 0x30: { // PUBLISH
                    int topic_len = (rx_buf[2] << 8) | rx_buf[3];
                    if (topic_len < 128) {
                        char topic[128];
                        memcpy(topic, &rx_buf[4], topic_len);
                        topic[topic_len] = '\0';
                        int payload_offset = 4 + topic_len;
                        int payload_len = r - payload_offset;
                        char payload[256] = {0};
                        if (payload_len < 256) {
                           memcpy(payload, &rx_buf[payload_offset], payload_len);
                           printf("[rx] Mensagem em '%s': '%s'\n", topic, payload);
                        }
                    }
                    break;
                }
                default:
                    printf("[rx] Pacote MQTT tipo 0x%X recebido.\n", packet_type);
                    break;
            }
        } else if (r <= 0) {
            printf("[rx_task] Erro de recebimento, finalizando tarefa.\n");
            rx_task_handle = NULL;
            vTaskDelete(NULL);
        }
    }
}

// --- TAREFA PRINCIPAL ---
static void main_task(void *pvParameters) {
    if (cyw43_arch_init()) { printf("[main] Falha ao inicializar cyw43_arch!\n"); }
    
    adc_init();
    adc_set_temp_sensor_enabled(true);
    adc_select_input(4);
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);

    cyw43_arch_enable_sta_mode();
    printf("[wifi] Conectando a '%s'...\n", WIFI_SSID);
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("[wifi] Falha crítica ao conectar.\n"); while(1);
    }
    printf("[wifi] Conectado com sucesso!\n");

    bool connected = false;
    unsigned char mqtt_buf[512];
    bool button_was_pressed = false;
    uint32_t last_temp_publish_time = 0;
    uint32_t last_comm_time = 0;

    while (1) {
        if (!connected) {
            printf("[app] Estado: DESCONECTADO. Tentando conectar...\n");

            if (rx_task_handle != NULL) { vTaskDelete(rx_task_handle); rx_task_handle = NULL; }
            
            if (mqtt_psk_client_init(&ctx, MQTT_HOST, MQTT_PORT, psk_key, psk_len, psk_identity, sizeof(psk_identity) - 1)) {
                int connect_len = build_mqtt_connect(MQTT_CLIENT_ID, mqtt_buf, sizeof(mqtt_buf));
                mqtt_psk_client_send(&ctx, mqtt_buf, connect_len);
                
                int received = mqtt_psk_client_recv(&ctx, mqtt_buf, sizeof(mqtt_buf));
                
                if (received == 4 && mqtt_buf[0] == 0x20 && mqtt_buf[1] == 0x02 && mqtt_buf[3] == 0x00) {
                    printf("\n>>> Conectado e autenticado no broker MQTT! <<<\n\n");
                    connected = true;
                    last_comm_time = to_ms_since_boot(get_absolute_time());
                    int sub_len = build_mqtt_subscribe(MQTT_TOPIC_SUBSCRIBE, mqtt_buf, sizeof(mqtt_buf));
                    mqtt_psk_client_send(&ctx, mqtt_buf, sub_len);
                    xTaskCreate(rx_task, "RxTask", RX_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, &rx_task_handle);
                } else {
                    printf("[app] Falha ao receber CONNACK. Verifique as credenciais e a configuração do broker.\n");
                    mqtt_psk_client_close(&ctx);
                }
            }
            if (!connected) vTaskDelay(pdMS_TO_TICKS(5000));
        }

        if (connected) {
            uint32_t now = to_ms_since_boot(get_absolute_time());
            bool message_sent = false;

            if (now > last_temp_publish_time + 30000) {
                float temp = read_mcu_temperature();
                char payload[16];
                snprintf(payload, sizeof(payload), "%.2f", temp);
                int len = build_mqtt_publish(MQTT_TOPIC_TEMP, payload, mqtt_buf, sizeof(mqtt_buf));
                printf("[app] Publicando temperatura: %s C\n", payload);
                if (mqtt_psk_client_send(&ctx, mqtt_buf, len) < 0) { connected = false; }
                else { message_sent = true; }
                last_temp_publish_time = now;
            }

            bool button_is_pressed = !gpio_get(BUTTON_PIN);
            if (button_is_pressed && !button_was_pressed) {
                int len = build_mqtt_publish(MQTT_TOPIC_BUTTON, "pressionado", mqtt_buf, sizeof(mqtt_buf));
                printf("[app] Publicando evento de botão.\n");
                if (mqtt_psk_client_send(&ctx, mqtt_buf, len) < 0) { connected = false; }
                else { message_sent = true; }
            }
            button_was_pressed = button_is_pressed;
            
            if (now > last_comm_time + (KEEP_ALIVE_SECONDS * 1000 / 2)) {
                if (!message_sent) {
                    int len = build_mqtt_pingreq(mqtt_buf, sizeof(mqtt_buf));
                    if (mqtt_psk_client_send(&ctx, mqtt_buf, len) < 0) { connected = false; }
                    else { message_sent = true; }
                }
            }
            if (message_sent) last_comm_time = now;
            if (!connected) { printf("[app] Conexão perdida.\n"); mqtt_psk_client_close(&ctx); }

            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, connected);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

// --- FUNÇÕES DE CONSTRUÇÃO DE PACOTES MQTT (CORRIGIDAS) ---
int encode_remaining_length(unsigned char *buf, int length) { int pos = 0; do { unsigned char encoded_byte = length % 128; length /= 128; if (length > 0) encoded_byte |= 128; buf[pos++] = encoded_byte; } while (length > 0); return pos; }
int build_mqtt_connect(const char *client_id, unsigned char *buf, int buf_len) { int client_id_len = strlen(client_id); int remaining_len = 10 + (2 + client_id_len); if (buf_len < 2 + remaining_len) return -1; int pos = 0; buf[pos++] = 0x10; unsigned char len_buf[4]; int len_bytes = encode_remaining_length(len_buf, remaining_len); memcpy(&buf[pos], len_buf, len_bytes); pos += len_bytes; const unsigned char var_header[] = { 0x00, 0x04, 'M', 'Q', 'T', 'T', 0x04, 0x02, (KEEP_ALIVE_SECONDS >> 8) & 0xFF, (KEEP_ALIVE_SECONDS & 0xFF) }; memcpy(&buf[pos], var_header, sizeof(var_header)); pos += sizeof(var_header); buf[pos++] = (client_id_len >> 8) & 0xFF; buf[pos++] = client_id_len & 0xFF; memcpy(&buf[pos], client_id, client_id_len); pos += client_id_len; return pos; }
int build_mqtt_publish(const char *topic, const char *payload, unsigned char *buf, int buf_len) { int topic_len = strlen(topic); int payload_len = strlen(payload); int remaining_len = 2 + topic_len + payload_len; if (buf_len < 5 + remaining_len) return -1; int pos = 0; buf[pos++] = 0x30; unsigned char len_buf[4]; int len_bytes = encode_remaining_length(len_buf, remaining_len); memcpy(&buf[pos], len_buf, len_bytes); pos += len_bytes; buf[pos++] = (topic_len >> 8) & 0xFF; buf[pos++] = topic_len & 0xFF; memcpy(&buf[pos], topic, topic_len); pos += topic_len; memcpy(&buf[pos], payload, payload_len); pos += payload_len; return pos; }
int build_mqtt_subscribe(const char* topic, unsigned char* buf, int buf_len) { int topic_len = strlen(topic); int remaining_len = 2 + 2 + topic_len + 1; if (buf_len < 2 + remaining_len) return -1; int pos = 0; buf[pos++] = 0x82; buf[pos++] = remaining_len; buf[pos++] = 0x00; buf[pos++] = 0x01; buf[pos++] = (topic_len >> 8) & 0xFF; buf[pos++] = topic_len & 0xFF; memcpy(&buf[pos], topic, topic_len); pos += topic_len; buf[pos++] = 0x00; return pos; }
int build_mqtt_pingreq(unsigned char *buf, int buf_len) { if (buf_len < 2) return -1; buf[0] = 0xC0; buf[1] = 0x00; return 2; }
float read_mcu_temperature() { const float conversion_factor = 3.3f / (1 << 12); adc_select_input(4); float adc = (float)adc_read() * conversion_factor; float temp_c = 27.0f - (adc - 0.706f) / 0.001721f; return temp_c; }

// --- MAIN ---
int main() {
    stdio_init_all();
    sleep_ms(2000); 
    printf("\n--- Tarefa 4 - Cliente MQTT PSK ---\n");
    xTaskCreate(main_task, "MainTask", MAIN_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY + 2, NULL);
    vTaskStartScheduler();
    while (true);
    return 0;
}