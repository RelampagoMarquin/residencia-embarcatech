#include "mqtt_handler.h"
#include <string.h>

// Includes do FreeRTOS e do Pico
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "pico/stdlib.h"
#include "lwip/apps/mqtt.h"

// Inclusão da sua nova biblioteca de Wi-Fi
#include "../wifi/wifi_conection.h"

// =================================================================================
//                          DEFINIÇÕES E VARIÁVEIS PRIVADAS
// =================================================================================

// Credenciais e Tópicos
#define MQTT_BROKER_HOST "mqtt.iot.natal.br"
#define MQTT_PORT 1883
#define MQTT_USER "desafio15"
#define MQTT_PASS "desafio15.laica"
#define MQTT_TOPIC_TEMP "ha/desafio15/marcos.silva/temp"
#define MQTT_TOPIC_JOY  "ha/desafio15/marcos.silva/joy" 

#define LED_PIN 12

// Variáveis estáticas (privadas para este arquivo)
static QueueHandle_t temperatureMqttQueue;
static QueueHandle_t joystickMqttQueue;

// Variáveis para guardar os parâmetros do display para a função conectar_wifi
static uint8_t g_display_width;
static uint8_t g_display_n_pages;
static int g_display_buffer_length;


// =================================================================================
//                            FUNÇÕES PRIVADAS (STATIC)
// =================================================================================

// (As funções mqtt_connection_cb, mqtt_pub_request_cb e publish_message)
static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status) {
    if (status == MQTT_CONNECT_ACCEPTED) {
        printf("MQTT: Conectado ao broker com sucesso!\n");
    } else {
        printf("MQTT: Falha na conexao, status: %d\n", status);
    }
}

static void mqtt_pub_request_cb(void *arg, err_t err) { /* Vazio */ }

static void publish_message(mqtt_client_t* client, const char* topic, const char* payload) {
    err_t err = mqtt_publish(client, topic, payload, strlen(payload), 0, 1, mqtt_pub_request_cb, NULL);
    if (err == ERR_OK) {
        printf("MQTT: Publicando no topico %s: %s\n", topic, payload);
        gpio_put(LED_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(50));
        gpio_put(LED_PIN, 0);
    } else {
        printf("MQTT: Erro ao publicar: %d\n", err);
    }
}

// A tarefa que gerencia o MQTT
static void vMqttTask(void *pvParameters) {
    // Agora chamamos a sua função da biblioteca de Wi-Fi
    printf("MQTT: Iniciando conexao Wi-Fi atraves da biblioteca...\n");
    if (conectar_wifi(g_display_width, g_display_n_pages, g_display_buffer_length) != 0) {
        printf("MQTT: Nao foi possivel conectar ao Wi-Fi. Deletando tarefa.\n");
        vTaskDelete(NULL); // Termina a tarefa se o Wi-Fi não conectar
    }
    printf("MQTT: Biblioteca Wi-Fi reportou conexao bem-sucedida.\n");

    // O resto da lógica MQTT continua igual
    mqtt_client_t* client = mqtt_client_new();
    if (client == NULL) {
        printf("MQTT: Falha ao criar cliente\n");
        vTaskDelete(NULL);
    }

    ip_addr_t broker_ip;
    ipaddr_aton(MQTT_BROKER_HOST, &broker_ip);

    struct mqtt_connect_client_info_t client_info = {
        .client_id = "pico_bitdoglab_aluno",
        .client_user = MQTT_USER,
        .client_pass = MQTT_PASS,
        .keep_alive = 60
    };

    mqtt_connect(client, &broker_ip, MQTT_PORT, mqtt_connection_cb, NULL, &client_info);

    for (;;) {
        // Loop de publicação
        int temp_to_publish;
        if (xQueueReceive(temperatureMqttQueue, &temp_to_publish, 0) == pdPASS) {
            char payload[8];
            snprintf(payload, sizeof(payload), "%d", temp_to_publish);
            publish_message(client, MQTT_TOPIC_TEMP, payload);
        }
        char joy_to_publish[16];
        if (xQueueReceive(joystickMqttQueue, &joy_to_publish, 0) == pdPASS) {
            publish_message(client, MQTT_TOPIC_JOY, joy_to_publish);
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

// =================================================================================
//                           IMPLEMENTAÇÃO DA INTERFACE PÚBLICA
// =================================================================================
void mqtt_handler_start(uint8_t width, uint8_t n_pages, int buffer_length) {
    // Guarda os parâmetros do display para serem usados pela vMqttTask
    g_display_width = width;
    g_display_n_pages = n_pages;
    g_display_buffer_length = buffer_length;
    
    temperatureMqttQueue = xQueueCreate(5, sizeof(int));
    joystickMqttQueue = xQueueCreate(5, sizeof(char[16]));
    
    xTaskCreate(vMqttTask, "MQTT_Task", 1024, NULL, 3, NULL);
}

// mqtt_handler_publish_temperature
void mqtt_handler_publish_temperature(int temp) {
    if (temperatureMqttQueue != NULL) {
        xQueueSend(temperatureMqttQueue, &temp, 0);
    }
}

// mqtt_handler_publish_joystick
void mqtt_handler_publish_joystick(const char* direction) {
    if (joystickMqttQueue != NULL) {
        xQueueSend(joystickMqttQueue, direction, 0);
    }
}