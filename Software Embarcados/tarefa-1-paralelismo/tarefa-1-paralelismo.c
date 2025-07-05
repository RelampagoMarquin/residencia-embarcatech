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

// === DEFINIÇÃO DE STRUCT ===
typedef struct {
    float temperatura;
    char direcao[16]; // Ex: "CIMA", "BAIXO", etc.
} DisplayData;

// === FILAS ===
QueueHandle_t displayQueue;

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
    const float conversion_factor = 3.3f / (1 << 12); // 12-bit ADC
    adc_select_input(4); // Canal 4 = temperatura interna
    uint16_t raw = adc_read();
    float voltage = raw * conversion_factor;
    return 27.0f - (voltage - 0.706f) / 0.001721f;
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
    snprintf(linha1, sizeof(linha1), "Temp: %.1f C", data.temperatura);
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
    static DisplayData lastData = {0, "REPOUSO"};
    for (;;) {
        lastData.temperatura = read_onboard_temperature();
        xQueueOverwrite(displayQueue, &lastData);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

// === TASK DE JOYSTICK ===
void vJoystickTask(void *pvParameters) {
    static DisplayData lastData = {0, "REPOUSO"};
    for (;;) {
        uint16_t vrx = 0, vry = 0;
        joystick_read_axis(&vrx, &vry);

        if (vry > 3500)
            strcpy(lastData.direcao, "CIMA");
        else if (vry < 500)
            strcpy(lastData.direcao, "BAIXO");
        else if (vrx > 3500)
            strcpy(lastData.direcao, "DIREITA");
        else if (vrx < 500)
            strcpy(lastData.direcao, "ESQUERDA");
        else
            strcpy(lastData.direcao, "REPOUSO");

        xQueueOverwrite(displayQueue, &lastData);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

// === TASK DE DISPLAY OLED ===
void vDisplayTask(void *pvParameters) {
    DisplayData received;
    for (;;) {
        if (xQueuePeek(displayQueue, &received, portMAX_DELAY) == pdTRUE) {
            display_menu(received);
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

    displayQueue = xQueueCreate(1, sizeof(DisplayData));
    if (displayQueue == NULL) {
        printf("Erro ao criar fila!\n");
        while (1);
    }

    xTaskCreate(vBlinkTask, "Blink", 128, NULL, 1, NULL);
    xTaskCreate(vSensorTask, "Sensor", 256, NULL, 1, NULL);
    xTaskCreate(vJoystickTask, "Joystick", 256, NULL, 1, NULL);
    xTaskCreate(vDisplayTask, "Display", 512, NULL, 2, NULL);

    vTaskStartScheduler();
    while (1);
}
