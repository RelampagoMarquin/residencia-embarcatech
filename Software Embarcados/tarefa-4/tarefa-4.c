#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"

#include <stdio.h>
#include <string.h>

// --- NOSSAS BIBLIOTECAS ---
// Display OLED
#include "inc/ssd1306.h" 
// A biblioteca de conexão PSK que você encontrou
#include "lib/mqtt_psk_client.h"
// Arquivo auxiliar para criar pacotes MQTT (vamos criá-lo a seguir)
#include "mqtt_helper.h" 

// === DEFINIÇÕES DE PINOS ===
#define BTN_A_PIN 5
#define I2C_SDA 14
#define I2C_SCL 15
#define LED_PIN 11 // Pino do LED da BitDogLab

// === CONFIGURAÇÕES DA TAREFA ===
#define WIFI_SSID "tarefa-mqtt"
#define WIFI_PASSWORD "laica@2025"
#define MQTT_SERVER "tarefa4.iot.natal.br"
#define MQTT_PORT 8849
#define MQTT_CLIENT_ID "aluno49-bitdoglab"
#define MQTT_PSK_IDENTITY "aluno49"
#define MQTT_PSK_KEY "ABCD49EF1234"
#define MQTT_TOPIC_TEMP "/aluno49/bitdoglab/temperatura"
#define MQTT_TOPIC_BTN "/aluno49/bitdoglab/botao"
#define MQTT_KEEP_ALIVE_S 60 // Segundos

// Struct do display
typedef struct {
    float temperatura;
    char botao_status[16];
    char conexao_status[16];
} DisplayData;

// --- VARIÁVEIS GLOBAIS ---
QueueHandle_t displayQueue;
// Contexto da nossa biblioteca de cliente MQTT
mqtt_client_context_t mqtt_client_ctx; 
bool mqtt_connected = false;

// === FUNÇÕES DE HARDWARE (sem alterações) ===
void init_oled() {
    i2c_init(i2c1, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    ssd1306_init();
}

void setup_button() {
    gpio_init(BTN_A_PIN);
    gpio_set_dir(BTN_A_PIN, GPIO_IN);
    gpio_pull_up(BTN_A_PIN);
}

float read_onboard_temperature() {
    adc_select_input(4);
    uint16_t raw = adc_read();
    float voltage = raw * (3.3f / (1 << 12));
    return 27.0f - (voltage - 0.706f) / 0.001721f;
}

void vBlinkTask() {
    gpio_put(LED_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_put(LED_PIN, 0);
}

// === TASK DE DISPLAY (com status da conexão) ===
void vDisplayTask(void *pvParameters) {
    DisplayData data;
    for (;;) {
        if (xQueuePeek(displayQueue, &data, portMAX_DELAY) == pdTRUE) {
            struct render_area area = {0, ssd1306_width - 1, 0, ssd1306_n_pages - 1};
            uint8_t buffer[ssd1306_buffer_length];
            memset(buffer, 0, sizeof(buffer));

            char linha1[32], linha2[32], linha3[32];
            snprintf(linha1, sizeof(linha1), "Temp: %.1f C", data.temperatura);
            snprintf(linha2, sizeof(linha2), "Botao: %s", data.botao_status);
            snprintf(linha3, sizeof(linha3), "MQTT: %s", data.conexao_status);

            ssd1306_draw_string(buffer, 0, 0, linha1);
            ssd1306_draw_string(buffer, 0, 12, linha2);
            ssd1306_draw_string(buffer, 0, 24, linha3);
            render_on_display(buffer, &area);
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

// === TASK DE TEMPERATURA (com nova lógica de publicação) ===
void vSensorTask(void *pvParameters) {
    for (;;) {
        float temp = read_onboard_temperature();

        DisplayData data;
        if (xQueuePeek(displayQueue, &data, 0) == pdTRUE) {
            data.temperatura = temp;
            xQueueOverwrite(displayQueue, &data);
        }

        if (mqtt_connected) {
            char payload[10];
            snprintf(payload, sizeof(payload), "%.1f", temp);

            // Monta e envia o pacote de publicação
            uint8_t buffer[128];
            int len = mqtt_helper_build_publish(buffer, sizeof(buffer), MQTT_TOPIC_TEMP, payload);
            if (len > 0) {
                mqtt_psk_client_send(&mqtt_client_ctx, buffer, len);
                printf("MQTT: Temperatura publicada: %s C\n", payload);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}

// === TASK DO BOTÃO (com nova lógica de publicação) ===
void vButtonTask(void *pvParameters) {
    bool last_state = true;
    for (;;) {
        bool current_state = gpio_get(BTN_A_PIN);
        if (last_state && !current_state) {
            vTaskDelay(pdMS_TO_TICKS(50)); // Debounce
            if (!gpio_get(BTN_A_PIN)) {
                printf("Botao pressionado!\n");

                DisplayData data;
                if (xQueuePeek(displayQueue, &data, 0) == pdTRUE) {
                    strcpy(data.botao_status, "PRESSIONADO");
                    xQueueOverwrite(displayQueue, &data);
                }

                if (mqtt_connected) {
                    const char *payload = "pressionado";
                    uint8_t buffer[128];
                    int len = mqtt_helper_build_publish(buffer, sizeof(buffer), MQTT_TOPIC_BTN, payload);
                    if (len > 0) {
                        mqtt_psk_client_send(&mqtt_client_ctx, buffer, len);
                        printf("MQTT: Botao publicado: %s\n", payload);
                        vBlinkTask();
                    }
                }
                vTaskDelay(pdMS_TO_TICKS(1000));
                if (xQueuePeek(displayQueue, &data, 0) == pdTRUE) {
                    strcpy(data.botao_status, "OK");
                    xQueueOverwrite(displayQueue, &data);
                }
            }
        }
        last_state = current_state;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// === [NOVO] TASK DE CONTROLE MQTT ===
void vMqttTask(void *pvParameters) {
    DisplayData data;

    while(1) {
        if (!mqtt_connected) {
            // Tenta conectar
             if (xQueuePeek(displayQueue, &data, 0) == pdTRUE) {
                strcpy(data.conexao_status, "Conectando...");
                xQueueOverwrite(displayQueue, &data);
            }

            // A função da sua biblioteca que inicia a conexão segura
            bool connected = mqtt_psk_client_init(&mqtt_client_ctx, MQTT_SERVER, MQTT_PORT,
                                                  (const unsigned char *)MQTT_PSK_KEY, strlen(MQTT_PSK_KEY),
                                                  (const unsigned char *)MQTT_PSK_IDENTITY, strlen(MQTT_PSK_IDENTITY));

            if (connected) {
                // Se conectou TCP+TLS, agora envia o pacote CONNECT do MQTT
                uint8_t buffer[128];
                int len = mqtt_helper_build_connect(buffer, sizeof(buffer), MQTT_CLIENT_ID, MQTT_KEEP_ALIVE_S);
                if (len > 0 && mqtt_psk_client_send(&mqtt_client_ctx, buffer, len) > 0) {
                     // Lógica para verificar o CONNACK (resposta de conexão)
                    memset(buffer, 0, sizeof(buffer));
                    int bytes_read = mqtt_psk_client_recv(&mqtt_client_ctx, buffer, 4);
                    // CONNACK tem 4 bytes, o segundo byte deve ser 0x02, o quarto deve ser 0x00
                    if (bytes_read == 4 && buffer[0] == 0x20 && buffer[1] == 0x02 && buffer[3] == 0x00) {
                        printf("MQTT: Conectado e autenticado!\n");
                        mqtt_connected = true;
                        vBlinkTask();
                        if (xQueuePeek(displayQueue, &data, 0) == pdTRUE) {
                            strcpy(data.conexao_status, "OK");
                            xQueueOverwrite(displayQueue, &data);
                        }
                    } else {
                        printf("MQTT: Falha no CONNACK. Resposta: %02x %02x %02x %02x\n", buffer[0], buffer[1], buffer[2], buffer[3]);
                        mqtt_psk_client_close(&mqtt_client_ctx);
                    }
                }
            } else {
                 printf("MQTT: Falha na conexão TCP/TLS.\n");
            }
        }

        if (mqtt_connected) {
            // Se conectado, envia PING para manter a conexão viva
            vTaskDelay(pdMS_TO_TICKS((MQTT_KEEP_ALIVE_S / 2) * 1000));
            
            uint8_t buffer[2] = {0xC0, 0x00}; // Pacote PINGREQ
            if(mqtt_psk_client_send(&mqtt_client_ctx, buffer, 2) <= 0) {
                // Erro ao enviar, provavelmente a conexão caiu
                printf("MQTT: Conexão perdida. Tentando reconectar...\n");
                mqtt_connected = false;
                mqtt_psk_client_close(&mqtt_client_ctx);
                if (xQueuePeek(displayQueue, &data, 0) == pdTRUE) {
                    strcpy(data.conexao_status, "Desconectado");
                    xQueueOverwrite(displayQueue, &data);
                }
            }
        } else {
            // Se não conectado, espera 5 segundos antes de tentar de novo
            vTaskDelay(pdMS_TO_TICKS(5000));
        }
    }
}

// === MAIN ===
int main() {
    stdio_init_all();
    adc_init();
    adc_set_temp_sensor_enabled(true);
    init_oled();
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    setup_button();

    displayQueue = xQueueCreate(1, sizeof(DisplayData));
    DisplayData initial = {0.0, "OK", "Iniciando..."};
    xQueueSend(displayQueue, &initial, 0);

    // Conexão Wi-Fi
    cyw43_arch_init();
    cyw43_arch_enable_sta_mode();
    printf("Conectando ao Wi-Fi: %s...\n", WIFI_SSID);
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("Falha na conexão Wi-Fi.\n");
        strcpy(initial.conexao_status, "Falha WiFi");
        xQueueOverwrite(displayQueue, &initial);
        while(1);
    }
    printf("Wi-Fi conectado.\n");

    // Criação das Tarefas
    xTaskCreate(vMqttTask, "MQTT_Task", 2048, NULL, 2, NULL);
    xTaskCreate(vSensorTask, "Sensor_Task", 1024, NULL, 1, NULL);
    xTaskCreate(vButtonTask, "Button_Task", 1024, NULL, 1, NULL);
    xTaskCreate(vDisplayTask, "Display_Task", 1024, NULL, 1, NULL);

    vTaskStartScheduler();
    while (1);
}
