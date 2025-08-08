#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "hardware/adc.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "inc/ssd1306.h"
#include <stdio.h>
#include <string.h>

// ===================================================================
// INCLUDES ADICIONADOS PARA WI-FI E MQTT
// ===================================================================
#include "pico/cyw43_arch.h"
#include "lwip/apps/mqtt.h"
#include "lwip/dns.h"
#include "lwip/ip_addr.h"
// ===================================================================

// === DEFINIÇÕES DE PINOS ===
#define VRX 26
#define VRY 27
#define SW 22
#define I2C_SDA 14
#define I2C_SCL 15
#define LED_PIN 11

// ===================================================================
// CONFIGURAÇÕES ADICIONADAS PARA WI-FI E MQTT
// ===================================================================
#define WIFI_SSID "COLOQUE O NOME DA SUA REDE"
#define WIFI_PASSWORD "SENHA DO SEU WIFI"
#define MQTT_SERVER "mqtt.iot.natal.br"
#define MQTT_PORT_CUSTOM 1883
#define MQTT_USER "desafio15"
#define MQTT_PASS "desafio15.laica"
#define MQTT_CLIENT_ID "marcos.silva-pico"
#define MQTT_TOPIC_TEMP "ha/desafio15/marcos.silva/temp"
#define MQTT_TOPIC_JOY "ha/desafio15/marcos.silva/joy"
// ===================================================================

// === DEFINIÇÃO DE STRUCT ===
typedef struct
{
    float temperatura;
    char direcao[16];
} DisplayData;

// === FILAS===
QueueHandle_t displayQueue;

// ===================================================================
// VARIÁVEIS GLOBAIS ADICIONADAS PARA MQTT
// ===================================================================
mqtt_client_t *client;
ip_addr_t mqtt_server_ip;
bool mqtt_connected = false;
// ===================================================================

// === FUNÇÕES DE HARDWARE ===
void init_oled()
{
    stdio_init_all();
    i2c_init(i2c1, ssd1306_i2c_clock * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    ssd1306_init();
}

float read_onboard_temperature()
{
    const float conversion_factor = 3.3f / (1 << 12);
    adc_select_input(4);
    uint16_t raw = adc_read();
    float voltage = raw * conversion_factor;
    return 27.0f - (voltage - 0.706f) / 0.001721f;
}

void setup_joystick()
{
    adc_init();
    adc_gpio_init(VRY);
    adc_gpio_init(VRX);
    gpio_init(SW);
    gpio_set_dir(SW, GPIO_IN);
    gpio_pull_up(SW);
}

void joystick_read_axis(uint16_t *vrx, uint16_t *vry)
{
    adc_select_input(0);
    *vry = adc_read();
    adc_select_input(1);
    *vrx = adc_read();
}

void display_menu(DisplayData data)
{
    struct render_area area = {0, ssd1306_width - 1, 0, ssd1306_n_pages - 1};
    calculate_render_area_buffer_length(&area);
    uint8_t buffer[ssd1306_buffer_length];
    memset(buffer, 0, sizeof(buffer));
    char linha1[32], linha2[32];
    snprintf(linha1, sizeof(linha1), "Temp: %.1f C", data.temperatura);
    snprintf(linha2, sizeof(linha2), "Direcao: %s", data.direcao);
    ssd1306_draw_string(buffer, 0, 8, linha1);
    ssd1306_draw_string(buffer, 0, 24, linha2);
    render_on_display(buffer, &area);
}

// ===================================================================
// FUNÇÕES DE CALLBACK ADICIONADAS PARA MQTT
// ===================================================================
void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status)
{
    if (status == MQTT_CONNECT_ACCEPTED)
    {
        printf("MQTT: Conectado com sucesso.\n");
        mqtt_connected = true;
    }
    else
    {
        printf("MQTT: Falha ao conectar. Codigo: %d\n", status);
        mqtt_connected = false;
    }
}

void dns_found_cb(const char *name, const ip_addr_t *ipaddr, void *callback_arg)
{
    if (ipaddr != NULL)
    {
        printf("MQTT: Broker %s resolvido para: %s\n", name, ipaddr_ntoa(ipaddr));
        ip_addr_copy(mqtt_server_ip, *ipaddr);
        client = mqtt_client_new();
        if (client != NULL)
        {
            struct mqtt_connect_client_info_t ci = {
                .client_id = MQTT_CLIENT_ID,
                .client_user = MQTT_USER,
                .client_pass = MQTT_PASS,
                .keep_alive = 60};
            printf("MQTT: Conectando ao broker...\n");
            mqtt_client_connect(client, &mqtt_server_ip, MQTT_PORT_CUSTOM, mqtt_connection_cb, NULL, &ci);
        }
    }
    else
    {
        printf("MQTT: Erro ao resolver o DNS do broker.\n");
    }
}
// ===================================================================

// === TASK DO LED ===
void vBlinkTask()
{
    gpio_put(LED_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_put(LED_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
}

// ===================================================================
// TASK DE SENSOR DE TEMPERATURA
// ===================================================================
void vSensorTask(void *pvParameters)
{
    for (;;)
    {
        float temp = read_onboard_temperature();

        // 1. Atualiza a fila para o display (lógica original)
        DisplayData current;
        if (xQueuePeek(displayQueue, &current, pdMS_TO_TICKS(50)) == pdTRUE)
        {
            current.temperatura = temp;
            xQueueOverwrite(displayQueue, &current);
        }

        // 2. Publica no MQTT (lógica adicionada)
        if (mqtt_connected)
        {
            char msg[16];
            snprintf(msg, sizeof(msg), "%.d", (int)temp);
            err_t err = mqtt_publish(client, MQTT_TOPIC_TEMP, msg, strlen(msg), 1, 1, NULL, NULL);
            if (err == ERR_OK)
            {
                printf("MQTT: Temperatura publicada: %s\n", msg);
                vBlinkTask();
            }
            else
            {
                printf("MQTT: Erro ao publicar temperatura: %d\n", err);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(30000)); // Aumenta o delay para 30s
    }
}
// ===================================================================

// ===================================================================
// TASK DE JOYSTICK
// ===================================================================
void vJoystickTask(void *pvParameters)
{
    char ultima_direcao_enviada[16] = ""; // Variável para guardar o último estado ENVIADO

    for (;;)
    {
        uint16_t vrx = 0, vry = 0;
        joystick_read_axis(&vrx, &vry);

        char nova_direcao[16] = "";
        if (vry > 3500)
            strcpy(nova_direcao, "CIMA");
        else if (vry < 500)
            strcpy(nova_direcao, "BAIXO");
        else if (vrx < 500)
            strcpy(nova_direcao, "ESQUERDA");
        else if (vrx > 3500)
            strcpy(nova_direcao, "DIREITA");

        // Apenas processa se a direção NÃO for de repouso
        if (strcmp(nova_direcao, "") != 0)
        {

            // 1. Atualiza a fila para o display
            DisplayData current;
            if (xQueuePeek(displayQueue, &current, pdMS_TO_TICKS(50)) == pdTRUE)
            {
                // Atualiza o display apenas se a direção mudou para evitar escritas desnecessárias
                if (strcmp(current.direcao, nova_direcao) != 0)
                {
                    strcpy(current.direcao, nova_direcao);
                    xQueueOverwrite(displayQueue, &current);
                }
            }

            // 2. Publica no MQTT somente se a direção mudou
            if (mqtt_connected && strcmp(nova_direcao, ultima_direcao_enviada) != 0)
            {
                err_t err = mqtt_publish(client, MQTT_TOPIC_JOY, nova_direcao, strlen(nova_direcao), 1, 1, NULL, NULL);
                if (err == ERR_OK)
                {
                    printf("MQTT: Joystick publicado: %s\n", nova_direcao);
                    strcpy(ultima_direcao_enviada, nova_direcao);
                    vBlinkTask();
                }
                else
                {
                    printf("MQTT: Erro ao publicar joystick: %d\n", err);
                }
            }
        }
        else
        {
            // Se a direção for "REPOUSO", reseta a "ultima_direcao_enviada"
            // para que o próximo movimento seja enviado.
            strcpy(ultima_direcao_enviada, "REPOUSO");
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
// ===================================================================

// === TASK DE DISPLAY OLED ===
void vDisplayTask(void *pvParameters)
{
    DisplayData data;
    for (;;)
    {
        if (xQueuePeek(displayQueue, &data, portMAX_DELAY) == pdTRUE)
        {
            display_menu(data);
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// === SETUP INICIAL ===
void setup(void)
{
    stdio_init_all();
    adc_init();
    adc_set_temp_sensor_enabled(true);
    init_oled();
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    setup_joystick();
}

// ===================================================================
// MAIN
// ===================================================================
int main()
{
    setup();
    adc_set_temp_sensor_enabled(true);
    displayQueue = xQueueCreate(1, sizeof(DisplayData));
    DisplayData initial = {0.0, "INICIANDO"};
    xQueueSend(displayQueue, &initial, 0);

    // --- Bloco de inicialização de Rede ---
    if (cyw43_arch_init())
    {
        printf("Erro ao inicializar o driver Wi-Fi.\n");
        return -1;
    }
    cyw43_arch_enable_sta_mode();
    printf("Conectando ao Wi-Fi: %s...\n", WIFI_SSID);
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000))
    {
        printf("Falha na conexão Wi-Fi.\n");
        return -1;
    }
    printf("Wi-Fi conectado com sucesso.\n");

    printf("Resolvendo broker MQTT...\n");
    dns_gethostbyname(MQTT_SERVER, &mqtt_server_ip, dns_found_cb, NULL);
    // ------------------------------------

    // Criação das Tarefas
    xTaskCreate(vSensorTask, "Sensor", 256, NULL, 1, NULL);
    xTaskCreate(vJoystickTask, "Joystick", 256, NULL, 1, NULL);
    xTaskCreate(vDisplayTask, "Display", 512, NULL, 2, NULL);

    // Inicia o escalonador do FreeRTOS
    vTaskStartScheduler();

    while (1);
}