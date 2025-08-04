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

// === DEFINIÇÕES DE PINOS ===

// === ANALOGICO ===
#define VRX 26
#define VRY 27
#define SW  22

// === DISPLAY ===
#define I2C_SDA 14
#define I2C_SCL 15

// === LED
#define LED_PIN 12

// // === DEFINIÇÕES MQTT ===
// #define MQTT_BROKER_HOST "mqtt.iot.natal.br"
// #define MQTT_PORT 1883

// // === ALTENTICACAO ===
// #define MQTT_USER "desafio15" 
// #define MQTT_PASS "desafio15.laica"

// // === CANAIS DO MQTT ===
// #define MQTT_TOPIC_TEMP "ha/desafio15/marcos.silva/temp"
// #define MQTT_TOPIC_JOY  "ha/desafio15/marcos.silva/joy"


// === DEFINIÇÃO DE STRUCT ===
typedef struct {
    float temperatura;
    char direcao[16]; // Ex: "CIMA", "BAIXO", etc.
} DisplayData;

// === FILAS ===
QueueHandle_t displayQueue; // Fila para o display
QueueHandle_t temperatureMqttQueue; // fila para enviar temperatura para o MQTT
QueueHandle_t joystickMqttQueue;   // fila para enviar direção do joystick para o MQTT


// === INIT OLED ===
void init_oled() {
    stdio_init_all();
    i2c_init(i2c1, ssd1306_i2c_clock * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    ssd1306_init();
}

// === LEITURA DO SENSOR DE TEMPERATURA INTERNA ===
float read_onboard_temperature() {
    const float conversion_factor = 3.3f / (1 << 12);
    adc_select_input(4);

    uint16_t raw = adc_read();
    float voltage = raw * conversion_factor;
    float temperature = 27.0f - (voltage - 0.706f) / 0.001721f;
    return temperature;
}

// === LEITURA DO JOYSTICK ===
void setup_joystick() {
    adc_init();
    adc_gpio_init(VRY); // canal 0
    adc_gpio_init(VRX); // canal 1
    gpio_init(SW);
    gpio_set_dir(SW, GPIO_IN);
    gpio_pull_up(SW);
}

void joystick_read_axis(uint16_t *vrx, uint16_t *vry) {
    adc_select_input(0); // VRY (Y)
    sleep_us(2);
    *vry = adc_read();

    adc_select_input(1); // VRX (X)
    sleep_us(2);
    *vrx = adc_read();
}

// === DISPLAY MENU ===
void display_menu(DisplayData data) {
    struct render_area area = {0, ssd1306_width - 1, 0, ssd1306_n_pages - 1};
    calculate_render_area_buffer_length(&area);
    uint8_t buffer[ssd1306_buffer_length];
    memset(buffer, 0, sizeof(buffer));

    char linha1[32], linha2[32];
    snprintf(linha1, sizeof(linha1), "Temp: %.1f °C", data.temperatura);
    snprintf(linha2, sizeof(linha2), "Direcao: %s", data.direcao);

    ssd1306_draw_string(buffer, 0, 8, linha1);   // Linha superior
    ssd1306_draw_string(buffer, 0, 24, linha2);  // Linha inferior

    render_on_display(buffer, &area);
}

// === TASK DO LED ===
void vBlinkTask(void *pvParameters) {
    for (;;) {
        gpio_put(LED_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(50));
        gpio_put(LED_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(950));
    }
}

// === TASK DE SENSOR DE TEMPERATURA ===
void vSensorTask(void *pvParameters) {
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(30000)); // Publicação a cada 30 segundos

        float temp_float = read_onboard_temperature();
        int temp_int = (int)temp_float; // Valor para o MQTT

        // 1. Envia o inteiro para a fila do MQTT
        if (temperatureMqttQueue != NULL) {
            xQueueSend(temperatureMqttQueue, &temp_int, 0);
        }

        // 2. Atualiza a struct na fila do Display (lógica que você já tinha)
        DisplayData current_display_data;
        if (xQueuePeek(displayQueue, &current_display_data, 0) == pdTRUE) {
            current_display_data.temperatura = temp_float; // Display pode ter float
            xQueueOverwrite(displayQueue, &current_display_data);
        }
    }
}

// === TASK DE JOYSTICK ===
void vJoystickTask(void *pvParameters) {
    char ultima_direcao_enviada[16] = ""; 
    
    for (;;) {
        uint16_t vrx = 0, vry = 0;
        joystick_read_axis(&vrx, &vry);

        char direcao_atual[16] = "";
        if (vry > 3500) strcpy(direcao_atual, "cima");
        else if (vry < 500) strcpy(direcao_atual, "baixo");
        else if (vrx < 500) strcpy(direcao_atual, "esquerda");
        else if (vrx > 3500) strcpy(direcao_atual, "direita");

        if (strcmp(direcao_atual, ultima_direcao_enviada) != 0 && strlen(direcao_atual) > 0) {
            // 1. Envia a string para a fila do MQTT
            if (joystickMqttQueue != NULL) {
                xQueueSend(joystickMqttQueue, &direcao_atual, 0);
            }

            // 2. Atualiza a struct na fila do Display
            DisplayData current_display_data;
            if (xQueuePeek(displayQueue, &current_display_data, 0) == pdTRUE) {
                strcpy(current_display_data.direcao, direcao_atual);
                xQueueOverwrite(displayQueue, &current_display_data);
            }
            
            strcpy(ultima_direcao_enviada, direcao_atual);
        }
        
        if (strlen(direcao_atual) == 0) {
            strcpy(ultima_direcao_enviada, "");
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// === TASK DE DISPLAY OLED ===
void vDisplayTask(void *pvParameters) {
    DisplayData data;
    for (;;) {
        if (xQueuePeek(displayQueue, &data, portMAX_DELAY) == pdTRUE) {
            display_menu(data);
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// === SETUP INICIAL ===
void setup(void) {
    stdio_init_all();
    adc_init();
    adc_set_temp_sensor_enabled(true);
    init_oled();
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    setup_joystick();
}

// === MAIN ===
int main() {
    setup();

    //seta como true a leitura do sensor
    adc_set_temp_sensor_enabled(true);

    displayQueue = xQueueCreate(1, sizeof(DisplayData));
    if (displayQueue == NULL) {
        printf("Erro ao criar fila!\n");
        while (1);
    }

    // Inicializa valor inicial na fila
    DisplayData initial = {0, "REPOUSO"};
    xQueueSend(displayQueue, &initial, portMAX_DELAY);

    xTaskCreate(vBlinkTask, "Blink", 128, NULL, 1, NULL);
    xTaskCreate(vSensorTask, "Sensor", 256, NULL, 1, NULL);
    xTaskCreate(vJoystickTask, "Joystick", 256, NULL, 1, NULL);
    xTaskCreate(vDisplayTask, "Display", 512, NULL, 2, NULL);

    vTaskStartScheduler();
    while (1);
}
