#include "mqtt_handler.h"
#include "../wifi/wifi_conection.h"
#include <string.h>

#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

#include "FreeRTOS.h"
#include "task.h"

#include "lwip/apps/mqtt.h"
#include "lwip/dns.h"

// --- SUAS CONFIGURAÇÕES ---
#define MQTT_BROKER_HOST "mqtt.iot.natal.br"
#define MQTT_BROKER_PORT 1883
#define MQTT_USER "desafio15"
#define MQTT_PASS "desafio15.laica"
#define MQTT_TOPIC_TEMP "ha/desafio15/marcos.silva/temp"
#define MQTT_TOPIC_JOY "ha/desafio15/marcos.silva/joy"
#define CLIENT_ID "pico-w-marcos-silva-rtos"

#define LED_PIN 12

// --- VARIÁVEIS GLOBAIS DO MÓDULO ---
static mqtt_client_t *mqtt_client_inst = NULL;
static bool mqtt_connected = false;

// --- FUNÇÕES DE CALLBACK (INTERNAS) ---
static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status)
{
    if (status == MQTT_CONNECT_ACCEPTED)
    {
        printf("MQTT Handler: Conexão com o broker estabelecida!\n");
        mqtt_connected = true;
    }
    else
    {
        printf("MQTT Handler: Falha na conexão, status: %d.\n", status);
        mqtt_connected = false;
    }
}

// --- FUNÇÕES PÚBLICAS (IMPLEMENTAÇÃO) ---

// Função chamada diretamente pela sua vSensorTask
void mqtt_handler_publish_temperature(int temp)
{
    if (!mqtt_connected || mqtt_client_inst == NULL)
    {
        return; // Não faz nada se não estiver conectado
    }

    char payload[8];
    snprintf(payload, sizeof(payload), "%d", temp);

    // Publica a mensagem
    err_t err = mqtt_publish(mqtt_client_inst, MQTT_TOPIC_TEMP, payload, strlen(payload), 1, 0, NULL, NULL);

    if (err == ERR_OK)
    {
        printf("MQTT Handler: Temperatura publicada: %s\n", payload);
        gpio_put(LED_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(50));
        gpio_put(LED_PIN, 0);
    }
    else
    {
        printf("MQTT Handler: Falha ao publicar temperatura, erro: %d\n", err);
    }
}

// Função chamada diretamente pela sua vJoystickTask
void mqtt_handler_publish_joystick(const char *direction)
{
    if (!mqtt_connected || mqtt_client_inst == NULL)
    {
        return; // Não faz nada se não estiver conectado
    }

    // Publica a mensagem
    err_t err = mqtt_publish(mqtt_client_inst, MQTT_TOPIC_JOY, direction, strlen(direction), 1, 0, NULL, NULL);

    if (err == ERR_OK)
    {
        printf("MQTT Handler: Joystick publicado: %s\n", direction);
        gpio_put(LED_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(50));
        gpio_put(LED_PIN, 0);
    }
    else
    {
        printf("MQTT Handler: Falha ao publicar joystick, erro: %d\n", err);
    }
}

// --- TAREFA DE GERENCIAMENTO DA CONEXÃO (INTERNA) ---
static void vMqttConnectionManagerTask(void *params)
{
    // 1. Conecta-se ao Wi-Fi primeiro
    uint8_t width = ssd1306_width;
    uint8_t n_pages = ssd1306_n_pages;
    int buffer_length;
    struct render_area frame_area = {0, width - 1, 0, n_pages - 1};
    calculate_render_area_buffer_length(&frame_area);
    buffer_length = ssd1306_buffer_length;

    // 1. Conecta-se ao Wi-Fi primeiro, agora com os parâmetros corretos
    if (conectar_wifi(width, n_pages, buffer_length) != 0)
    {
        printf("MQTT Handler: Falha ao conectar ao Wi-Fi. Abortando tarefa MQTT.\n");
        vTaskDelete(NULL);
        return;
    }

    // 2. Inicializa o cliente LwIP
    mqtt_client_inst = mqtt_client_new();
    if (mqtt_client_inst == NULL)
    {
        printf("MQTT Handler: Falha ao criar cliente MQTT.\n");
        vTaskDelete(NULL);
        return;
    }

    // 3. Configura as informações de conexão
    struct mqtt_connect_client_info_t ci;
    memset(&ci, 0, sizeof(ci));
    ci.client_id = CLIENT_ID;
    ci.client_user = MQTT_USER;
    ci.client_pass = MQTT_PASS;
    ci.keep_alive = 60;

    // 4. Resolve o IP do broker
    ip_addr_t broker_ip;
    err_t err = dns_gethostbyname(MQTT_BROKER_HOST, &broker_ip, NULL, NULL);
    while (err != ERR_OK)
    {
        printf("MQTT Handler: Falha ao resolver DNS. Tentando novamente em 5s...\n");
        vTaskDelay(pdMS_TO_TICKS(5000));
        err = dns_gethostbyname(MQTT_BROKER_HOST, &broker_ip, NULL, NULL);
    }
    printf("MQTT Handler: Broker IP resolvido: %s\n", ipaddr_ntoa(&broker_ip));

    // 5. Tenta conectar ao broker MQTT em loop
    while (true)
    {
        if (!mqtt_client_is_connected(mqtt_client_inst))
        {
            printf("MQTT Handler: Conectando ao broker...\n");
            mqtt_client_connect(mqtt_client_inst, &broker_ip, MQTT_BROKER_PORT, mqtt_connection_cb, 0, &ci);
        }
        // Aguarda 5 segundos antes de verificar a conexão novamente
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

// --- FUNÇÃO DE INICIALIZAÇÃO PÚBLICA ---
void mqtt_handler_init(void)
{
    // Cria apenas a tarefa que gerencia a conexão.
    // A publicação agora é feita por chamada direta.
    xTaskCreate(vMqttConnectionManagerTask, "MqttConnManager", configMINIMAL_STACK_SIZE * 4, NULL, tskIDLE_PRIORITY + 3, NULL);
}