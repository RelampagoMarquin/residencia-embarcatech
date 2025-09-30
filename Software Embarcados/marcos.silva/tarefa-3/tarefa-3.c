#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "lwip/apps/mqtt.h"
#include "lwip/dns.h"
#include "lwip/ip_addr.h"
#include "inc/ssd1306.h"
#include "inc/mpu6050_handler.h"
#include "inc/ntp_client.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

// ===================================================================
// CONFIGURAÇÕES
// ===================================================================
#define WIFI_SSID "SUA REDE WIFI"
#define WIFI_PASSWORD "SUA SENHA DO WIFI"
#define DESAFIO_NUM "15"
#define SEU_NOME "marcos.silva"
#define MQTT_SERVER "mqtt.iot.natal.br"
#define MQTT_PORT_CUSTOM 1883
#define MQTT_USER "desafio" DESAFIO_NUM
#define MQTT_PASS "desafio" DESAFIO_NUM ".laica"
#define MQTT_CLIENT_ID SEU_NOME "-pico"
#define MQTT_TOPIC "ha/desafio" DESAFIO_NUM "/" SEU_NOME "/mpu6050"

// === PINOS ===
#define I2C0_SDA 0
#define I2C0_SCL 1
#define I2C1_SDA 14
#define I2C1_SCL 15
#define LED_PIN_GREEN 11 // Led Verde
#define LED_PIN_RED 13   // Led vermelho

// === FILAS E STRUCTS (sem alterações) ===
QueueHandle_t displayQueue;
typedef struct
{
    float ax, ay, az;
    float gx, gy, gz;
    float temp;
} DisplayData;

//=== NTP =====

#define NTP_SERVER "pool.ntp.br" // Melhor usar o pool brasileiro
#define NTP_TIMEOUT_MS 5000
#define NTP_RESYNC_INTERVAL_MS 3600000      // Ressincronizar a cada 1 hora (3600 * 1000)
#define BRASILIA_OFFSET_SECONDS (-3 * 3600) // Fuso horário de Brasília (UTC-3)

// === GLOBAIS DE TEMPO (NOVAS) ===
volatile bool time_synchronized = false;
volatile time_t current_utc_time = 0;

// === GLOBAIS MQTT (sem alterações) ===
mqtt_client_t *client;
ip_addr_t mqtt_server_ip;
bool mqtt_connected = false;

// === FUNÇÕES DE DISPLAY (sem alterações) ===
void display_message(const char *line1, const char *line2, const char *line3, const char *line4)
{
    struct render_area area = {0, ssd1306_width - 1, 0, ssd1306_n_pages - 1};
    calculate_render_area_buffer_length(&area);
    uint8_t buffer[ssd1306_buffer_length];
    memset(buffer, 0, sizeof(buffer));
    if (line1)
        ssd1306_draw_string(buffer, 0, 0, (char *)line1);
    if (line2)
        ssd1306_draw_string(buffer, 0, 16, (char *)line2);
    if (line3)
        ssd1306_draw_string(buffer, 0, 32, (char *)line3);
    if (line4)
        ssd1306_draw_string(buffer, 0, 48, (char *)line4);
    render_on_display(buffer, &area);
}

// ===================================================================
// MUDANÇA: FUNÇÃO DE INICIALIZAÇÃO ADAPTADA
// ===================================================================
void init_all_peripherals()
{
    stdio_init_all();

    // I2C 0 para MPU-6050
    i2c_init(i2c0, 400 * 1000);
    gpio_set_function(I2C0_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C0_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C0_SDA);
    gpio_pull_up(I2C0_SCL);

    // Usando a nova função de inicialização com as faixas padrão
    // Isso define a sensibilidade do sensor
    if (!mpu6050_init(i2c0, GYRO_FS_250_DPS, ACCEL_FS_2G))
    {
        printf("Falha ao inicializar o MPU6050!\n");
    }

    // I2C 1 para Display OLED
    i2c_init(i2c1, 400 * 1000);
    gpio_set_function(I2C1_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C1_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C1_SDA);
    gpio_pull_up(I2C1_SCL);
    ssd1306_init(i2c1);

    gpio_init(LED_PIN_GREEN);
    gpio_set_dir(LED_PIN_GREEN, GPIO_OUT);

    gpio_init(LED_PIN_RED);
    gpio_set_dir(LED_PIN_RED, GPIO_OUT);

    display_message("Hardware OK", "Iniciando WiFi...", NULL, NULL);
}

// === FUNÇÕES MQTT (sem alteração) ===
void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status)
{
    if (status == MQTT_CONNECT_ACCEPTED)
    {
        printf("MQTT: Conectado.\n");
        mqtt_connected = true;
    }
    else
    {
        printf("MQTT: Falha conexao: %d\n", status);
        mqtt_connected = false;
    }
}
void dns_found_cb(const char *name, const ip_addr_t *ipaddr, void *callback_arg)
{
    if (ipaddr != NULL)
    {
        ip_addr_copy(mqtt_server_ip, *ipaddr);
        client = mqtt_client_new();
        if (client != NULL)
        {
            struct mqtt_connect_client_info_t ci = {
                .client_id = MQTT_CLIENT_ID, .client_user = MQTT_USER, .client_pass = MQTT_PASS, .keep_alive = 60};
            mqtt_client_connect(client, &mqtt_server_ip, MQTT_PORT_CUSTOM, mqtt_connection_cb, NULL, &ci);
        }
    }
    else
    {
        printf("MQTT: Erro DNS.\n");
    }
}

// === TASKS DO FREERTOS ===
void vBlinkLed(u8_t LED_PIN)
{
    gpio_put(LED_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_put(LED_PIN, 0);
}

void vDisplayTask(void *pvParameters)
{
    DisplayData data;
    char line1_temp[20];
    char line2_header[20];
    char line3_accel[20];
    char line4_gyro[20];

    for (;;)
    {
        if (xQueueReceive(displayQueue, &data, portMAX_DELAY) == pdTRUE)
        {
            // Linha 1: Temperatura
            snprintf(line1_temp, sizeof(line1_temp), "Temp: %.0f C", data.temp);

            // Linha 2: Cabeçalho (sem alteração)
            strcpy(line2_header, "   x /   y /   z");

            // Linha 3: Dados do Acelerômetro
            snprintf(line3_accel, sizeof(line3_accel), "A:%.1f/%.1f/%.1f", data.ax, data.ay, data.az);

            // Linha 4: Dados do Giroscópio
            snprintf(line4_gyro, sizeof(line4_gyro), "G:%4.0f/%4.0f/%4.0f", data.gx, data.gy, data.gz);
            
            // Envia as 4 linhas formatadas para a função de display
            display_message(line1_temp, line2_header, line3_accel, line4_gyro);
        }
    }
}

// ===================================================================
// TASK DO SENSOR
// ===================================================================
void vMpuSensorTask(void *pvParameters)
{
    mpu6050_data_t sensor_data;
    char payload[512];
    char timestamp_str[20];
    TickType_t last_publish_time = xTaskGetTickCount();
    mpu6050_data_t last_published_data = {0};

    for (;;)
    {
        if (mpu6050_read_data(&sensor_data))
        {
            DisplayData display_info = {
                .ax = sensor_data.accel_x,
                .ay = sensor_data.accel_y,
                .az = sensor_data.accel_z,
                .gx = sensor_data.gyro_x,
                .gy = sensor_data.gyro_y,
                .gz = sensor_data.gyro_z,
                .temp = sensor_data.temperature};
            xQueueOverwrite(displayQueue, &display_info);

            bool has_changed = (fabs(sensor_data.accel_x - last_published_data.accel_x) > 0.1);
            TickType_t current_time_ticks = xTaskGetTickCount();
            if ((current_time_ticks - last_publish_time > pdMS_TO_TICKS(10000) && has_changed) || (current_time_ticks - last_publish_time > pdMS_TO_TICKS(60000)))
            {
                if (mqtt_connected && mqtt_client_is_connected(client))
                {
                    if (time_synchronized)
                    {
                        time_t brasili_time = current_utc_time + BRASILIA_OFFSET_SECONDS;
                        struct tm *local_tm = gmtime(&brasili_time);
                        strftime(timestamp_str, sizeof(timestamp_str), "%Y-%m-%dT%H:%M:%S", local_tm);
                    }
                    else
                    {
                        strcpy(timestamp_str, "1970-01-01T00:00:00");
                    }

                    snprintf(payload, sizeof(payload), "{\"team\":\"desafio%s\",\"device\":\"bitdoglab_%s\",\"ip\":\"%s\",\"ssid\":\"%s\",\"sensor\":\"MPU-6050\",\"data\":{\"accel\":{\"x\":%.2f,\"y\":%.2f,\"z\":%.2f},\"gyro\":{\"x\":%.2f,\"y\":%.2f,\"z\":%.2f},\"temperature\":%.1f},\"timestamp\":\"%s\"}",
                             DESAFIO_NUM, SEU_NOME,
                             ip4addr_ntoa(netif_ip4_addr(netif_default)), WIFI_SSID,
                             sensor_data.accel_x, sensor_data.accel_y, sensor_data.accel_z,
                             sensor_data.gyro_x, sensor_data.gyro_y, sensor_data.gyro_z,
                             sensor_data.temperature, timestamp_str);

                    if (mqtt_publish(client, MQTT_TOPIC, payload, strlen(payload), 0, 0, NULL, NULL) == ERR_OK)
                    {
                        printf("MQTT: Publicado com timestamp: %s\n", timestamp_str);
                        vBlinkLed(LED_PIN_GREEN);
                        last_publish_time = current_time_ticks;
                        last_published_data = sensor_data;
                    }
                    else
                    {
                        printf("MQTT: Erro ao publicar.\n");
                        vBlinkLed(LED_PIN_RED);
                    }
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// ===================================================================
// TASK PARA GERENCIAR O NTP
// ===================================================================
void vNtpTask(void *pvParameters)
{
    // Aguarda um pouco no início para garantir que o Wi-Fi está 100% estável
    vTaskDelay(pdMS_TO_TICKS(10000));

    for (;;)
    {
        printf("NTP Task: Tentando sincronizar a hora...\n");
        display_message("WiFi OK", "Sincronizando", "Hora (NTP)...", NULL);

        // Chama a função bloqueante da sua nova biblioteca
        if (ntp_get_time(NTP_SERVER, NTP_TIMEOUT_MS))
        {
            // Se funcionou, pega o tempo e atualiza as variáveis globais
            current_utc_time = ntp_get_last_time();
            time_synchronized = true;

            // Converte para hora local apenas para exibir no terminal
            time_t brasili_time = current_utc_time + BRASILIA_OFFSET_SECONDS;
            struct tm *local_tm = gmtime(&brasili_time);
            printf("NTP Task: Hora sincronizada com sucesso: %s", asctime(local_tm));
        }
        else
        {
            printf("NTP Task: Falha ao sincronizar a hora.\n");
            time_synchronized = false;
        }

        // Dorme por 1 hora antes de tentar ressincronizar
        vTaskDelay(pdMS_TO_TICKS(NTP_RESYNC_INTERVAL_MS));
    }
}

// === MAIN ===
int main()
{
    init_all_peripherals();
    displayQueue = xQueueCreate(1, sizeof(DisplayData));
    if (cyw43_arch_init())
    {
        display_message("ERRO", "Falha no WiFi", NULL, NULL);
        return -1;
    }
    cyw43_arch_enable_sta_mode();
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000))
    {
        display_message("ERRO", "WiFi Nao Conecta", NULL, NULL);
        return -1;
    }
    display_message("Hardware OK", "WiFi OK", "Conectando MQTT", ip4addr_ntoa(netif_ip4_addr(netif_default)));
    dns_gethostbyname(MQTT_SERVER, &mqtt_server_ip, dns_found_cb, NULL);

    // Criando as três tasks
    xTaskCreate(vMpuSensorTask, "MPU_Task", 1024, NULL, 1, NULL);
    xTaskCreate(vDisplayTask, "Display_Task", 512, NULL, 2, NULL);
    xTaskCreate(vNtpTask, "NTP_Task", 1024, NULL, 0, NULL);

    vTaskStartScheduler();
    while (1)
        ;
}