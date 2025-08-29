#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"

// Inclusões para periféricos e protocolos
#include "lwip/apps/mqtt.h"
#include "lwip/dns.h"
#include "lwip/ip_addr.h"
#include "inc/ssd1306.h"
#include "inc/mpu6050.h" // <-- BIBLIOTECA DO SENSOR MPU-6050
#include <stdio.h>
#include <string.h>
#include <math.h>

// ===================================================================
// 1. AJUSTE SUAS CONFIGURAÇÕES AQUI
// ===================================================================
#define WIFI_SSID       "SUA_REDE_WIFI"
#define WIFI_PASSWORD   "SUA_SENHA_WIFI"

#define DESAFIO_NUM     "15" // <-- SEU NÚMERO DE DESAFIO (ex: "01", "07", "15")
#define SEU_NOME        "marcos.silva" // <-- SEU NOME no formato "primeiro.ultimo"

#define MQTT_SERVER     "mqtt.iot.natal.br"
#define MQTT_PORT_CUSTOM 1883
#define MQTT_USER       "desafio" DESAFIO_NUM
#define MQTT_PASS       "desafio" DESAFIO_NUM ".laica"
#define MQTT_CLIENT_ID  SEU_NOME "-pico"
#define MQTT_TOPIC      "ha/desafio" DESAFIO_NUM "/" SEU_NOME "/mpu6050"
// ===================================================================

// === DEFINIÇÕES DE PINOS ===
#define I2C_SDA 4 // Pino SDA padrão para I2C0
#define I2C_SCL 5 // Pino SCL padrão para I2C0
#define LED_PIN 11

// === FILAS E STRUCTS ===
QueueHandle_t displayQueue;

// Struct para os dados exibidos no OLED (simplificado)
typedef struct {
    float ax;
    float ay;
    float temp;
} DisplayData;

// === VARIÁVEIS GLOBAIS MQTT ===
mqtt_client_t *client;
ip_addr_t mqtt_server_ip;
bool mqtt_connected = false;

// === FUNÇÕES DE HARDWARE E DISPLAY ===
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

void init_peripherals() {
    stdio_init_all();
    init_oled();

    // Inicializa MPU-6050
    mpu6050_reset(i2c0);

    // LED de feedback
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
}

void display_task_function(DisplayData data) {
    struct render_area area = {0, ssd1306_width - 1, 0, ssd1306_n_pages - 1};
    calculate_render_area_buffer_length(&area);
    uint8_t buffer[ssd1306_buffer_length];
    memset(buffer, 0, sizeof(buffer));

    char linha1[32], linha2[32], linha3[32];
    snprintf(linha1, sizeof(linha1), "Ax: %.2f g", data.ax);
    snprintf(linha2, sizeof(linha2), "Ay: %.2f g", data.ay);
    snprintf(linha3, sizeof(linha3), "Temp: %.1f C", data.temp);

    ssd1306_draw_string(buffer, 0, 0,  "MPU-6050 Data");
    ssd1306_draw_string(buffer, 0, 16, linha1);
    ssd1306_draw_string(buffer, 0, 32, linha2);
    ssd1306_draw_string(buffer, 0, 48, linha3);

    render_on_display(buffer, &area);
}

// === FUNÇÕES DE CALLBACK E CONEXÃO MQTT (sem alterações) ===
void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status) {
    if (status == MQTT_CONNECT_ACCEPTED) {
        printf("MQTT: Conectado com sucesso.\n");
        mqtt_connected = true;
    } else {
        printf("MQTT: Falha ao conectar. Codigo: %d\n", status);
        mqtt_connected = false;
    }
}

void dns_found_cb(const char *name, const ip_addr_t *ipaddr, void *callback_arg) {
    if (ipaddr != NULL) {
        printf("MQTT: Broker %s resolvido para: %s\n", name, ipaddr_ntoa(ipaddr));
        ip_addr_copy(mqtt_server_ip, *ipaddr);
        client = mqtt_client_new();
        if (client != NULL) {
            struct mqtt_connect_client_info_t ci = {
                .client_id = MQTT_CLIENT_ID,
                .client_user = MQTT_USER,
                .client_pass = MQTT_PASS,
                .keep_alive = 60
            };
            printf("MQTT: Conectando ao broker...\n");
            mqtt_client_connect(client, &mqtt_server_ip, MQTT_PORT_CUSTOM, mqtt_connection_cb, NULL, &ci);
        }
    } else {
        printf("MQTT: Erro ao resolver o DNS do broker.\n");
    }
}

// === TASKS DO FREERTOS ===

// Task para piscar o LED
void vBlinkLed() {
    gpio_put(LED_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_put(LED_PIN, 0);
}

// Task para o Display OLED
void vDisplayTask(void *pvParameters) {
    DisplayData data;
    for (;;) {
        // Aguarda novos dados na fila para exibir
        if (xQueueReceive(displayQueue, &data, portMAX_DELAY) == pdTRUE) {
            display_task_function(data);
        }
    }
}

// 2. NOVA TASK PARA O SENSOR MPU-6050 E MQTT
void vMpuSensorTask(void *pvParameters) {
    mpu_data_t sensor_data;
    char payload[512]; // Buffer para a mensagem JSON
    
    // Lógica de tempo para publicação (conforme requisitado)
    TickType_t last_publish_time = xTaskGetTickCount();
    mpu_data_t last_published_data = {0};
    const TickType_t min_interval = pdMS_TO_TICKS(10000); // 10 segundos
    const TickType_t max_interval = pdMS_TO_TICKS(60000); // 60 segundos

    for (;;) {
        // 1. Lê os dados do sensor
        mpu6050_get_data(i2c0, &sensor_data);
        
        // 2. Envia dados para a fila do display
        DisplayData display_info = { .ax = sensor_data.accel_x, .ay = sensor_data.accel_y, .temp = sensor_data.temp };
        xQueueOverwrite(displayQueue, &display_info);

        // 3. Verifica se deve publicar no MQTT
        bool has_changed = (fabs(sensor_data.accel_x - last_published_data.accel_x) > 0.1); // Exemplo de critério de mudança
        TickType_t current_time = xTaskGetTickCount();
        
        if ((current_time - last_publish_time > min_interval && has_changed) || (current_time - last_publish_time > max_interval)) {
            if (mqtt_connected) {
                // 4. Monta o payload JSON
                // OBS: O timestamp real requer um cliente NTP. Aqui usamos um placeholder.
                snprintf(payload, sizeof(payload),
                         "{\"team\":\"desafio%s\",\"device\":\"bitdoglab_%s\",\"ip\":\"%s\",\"ssid\":\"%s\","
                         "\"sensor\":\"MPU-6050\",\"data\":{\"accel\":{\"x\":%.2f,\"y\":%.2f,\"z\":%.2f},"
                         "\"gyro\":{\"x\":%.2f,\"y\":%.2f,\"z\":%.2f},\"temperature\":%.1f},"
                         "\"timestamp\":\"2025-08-22T10:15:20\"}",
                         DESAFIO_NUM, SEU_NOME,
                         ip4addr_ntoa(netif_ip4_addr(netif_default)), WIFI_SSID,
                         sensor_data.accel_x, sensor_data.accel_y, sensor_data.accel_z,
                         sensor_data.gyro_x, sensor_data.gyro_y, sensor_data.gyro_z,
                         sensor_data.temp);

                // 5. Publica a mensagem
                // QoS = 0, Retain = 0
                err_t err = mqtt_publish(client, MQTT_TOPIC, payload, strlen(payload), 0, 0, NULL, NULL);

                if (err == ERR_OK) {
                    printf("MQTT: Payload publicado com sucesso!\n%s\n", payload);
                    vBlinkLed(); // Pisca o LED para indicar sucesso
                    last_publish_time = current_time; // Atualiza o tempo da última publicação
                    last_published_data = sensor_data; // Atualiza os dados da última publicação
                } else {
                    printf("MQTT: Erro ao publicar. Codigo: %d\n", err);
                }
            }
        }
        
        // Aguarda 1 segundo antes da próxima leitura (não afeta o intervalo de publicação)
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// === MAIN ===
int main() {
    init_peripherals();

    displayQueue = xQueueCreate(1, sizeof(DisplayData));
    DisplayData initial_data = {0.0, 0.0, 0.0};
    xQueueSend(displayQueue, &initial_data, 0);

    // --- Conexão Wi-Fi e MQTT ---
    if (cyw43_arch_init()) {
        printf("Erro ao inicializar o driver Wi-Fi.\n");
        return -1;
    }
    cyw43_arch_enable_sta_mode();
    printf("Conectando ao Wi-Fi: %s...\n", WIFI_SSID);
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("Falha na conexão Wi-Fi.\n");
        return -1;
    }
    printf("Wi-Fi conectado com sucesso! IP: %s\n", ip4addr_ntoa(netif_ip4_addr(netif_default)));

    printf("Resolvendo broker MQTT...\n");
    dns_gethostbyname(MQTT_SERVER, &mqtt_server_ip, dns_found_cb, NULL);
    
    // Aguarda um pouco para a conexão MQTT ser estabelecida
    vTaskDelay(pdMS_TO_TICKS(5000));

    // 3. CRIAÇÃO DAS NOVAS TASKS
    xTaskCreate(vMpuSensorTask, "MPU_Sensor_Task", 1024, NULL, 1, NULL);
    xTaskCreate(vDisplayTask, "Display_Task", 512, NULL, 2, NULL);

    vTaskStartScheduler();

    while (1);
}