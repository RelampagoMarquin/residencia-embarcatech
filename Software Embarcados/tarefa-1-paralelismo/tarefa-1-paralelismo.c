#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"

#include "inc/ssd1306.h"

// --- Definições de Pinos ---
// LED
const uint LED_PIN = 12;

// Analogico
const uint VRX_PIN = 26; // Eixo X, ADC0
const uint VRY_PIN = 27; // Eixo Y, ADC1

// display
const uint I2C_SDA = 14;
const uint I2C_SCL = 15;

// --- Constantes ---
const uint16_t JOYSTICK_THRESHOLD_LOW = 500;
const uint16_t JOYSTICK_THRESHOLD_HIGH = 3500;

// --- Filas para Comunicação entre Tarefas ---
QueueHandle_t xTemperatureQueue;
QueueHandle_t xJoystickQueue;

// --- Funções de Hardware e Display ---

void setup_joystick()
{
    adc_init();
    adc_gpio_init(VRX_PIN);
    adc_gpio_init(VRY_PIN);
}

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

void update_display()
{
    char temp_str[20];
    char joy_str[20];

    float current_temp = 0.0;
    char current_joy_dir[15] = "Repouso";

    // Formata as strings para exibição
    sprintf(temp_str, "Temp: %.1f C", current_temp);
    sprintf(joy_str, "Joy: %s", current_joy_dir);

    struct render_area frame_area = {0, ssd1306_width - 1, 0, ssd1306_n_pages - 1};
    calculate_render_area_buffer_length(&frame_area);
    static uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, ssd1306_buffer_length);

    ssd1306_draw_string(0, 0, 1, temp_str); // Escreve na linha 1
    ssd1306_draw_string(0, 16, 1, joy_str); // Escreve na linha 2

    render_on_display(ssd, &frame_area);
}

// --- Tarefas FreeRTOS ---

/**
 * @brief Tarefa para piscar o LED.
 * Requisito 2.1: LED aceso por 50ms, apagado por 950ms.
 */
void led_task(void *pvParameters)
{
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    for (;;)
    {
        gpio_put(LED_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(50));
        gpio_put(LED_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(950));
    }
}

/**
 * @brief Tarefa para ler a temperatura interna do MCU.
 * Requisito 2.2: Leitura a cada 2 segundos e exibição.
 */
void temperature_task(void *pvParameters)
{
    adc_set_temp_sensor_enabled(true);
    for (;;)
    {
        adc_select_input(4);
        uint16_t raw_value = adc_read();

        const float conversion_factor = 3.3f / (1 << 12);
        float voltage = raw_value * conversion_factor;
        float temperature = 27.0f - (voltage - 0.706f) / 0.001721f;

        // Envia o dado da temperatura para a fila, sem se preocupar em como será exibido
        xQueueSend(xTemperatureQueue, &temperature, portMAX_DELAY);

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

/**
 * @brief Tarefa para ler a direção do joystick.
 * Requisito 2.3: Detecta a última direção válida pressionada.
 */
void joystick_task(void *pvParameters)
{
    char last_direction[15] = "Repouso";
    for (;;)
    {
        adc_select_input(0);
        uint16_t vrx_value = adc_read();
        adc_select_input(1);
        uint16_t vry_value = adc_read();

        char current_direction[15] = "Repouso";

        if (vry_value < JOYSTICK_THRESHOLD_LOW)
            sprintf(current_direction, "Cima");
        else if (vry_value > JOYSTICK_THRESHOLD_HIGH)
            sprintf(current_direction, "Baixo");
        else if (vrx_value < JOYSTICK_THRESHOLD_LOW)
            sprintf(current_direction, "Esquerda");
        else if (vrx_value > JOYSTICK_THRESHOLD_HIGH)
            sprintf(current_direction, "Direita");

        if (strcmp(last_direction, current_direction) != 0)
        {
            strcpy(last_direction, current_direction);
            // Envia a nova direção para a fila
            xQueueSend(xJoystickQueue, &last_direction, portMAX_DELAY);
        }

        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

/**
 * @brief Tarefa dedicada a receber dados e atualizar o display OLED.
 */
// void display_task(void *pvParameters)
// {
//     // Variáveis locais para armazenar o estado mais recente dos dados
//     float current_temp = 0.0;
//     char current_joy_dir[15] = "Repouso";

//     // Atualiza a tela uma vez no início
//     update_display(current_temp, current_joy_dir);

//     for (;;)
//     {
//         float received_temp;
//         char received_joy_dir[15];
//         bool screen_needs_update = false;

//         // Tenta receber da fila de temperatura (sem bloquear)
//         if (xQueueReceive(xTemperatureQueue, &received_temp, 0) == pdTRUE)
//         {
//             current_temp = received_temp;
//             screen_needs_update = true;
//         }

//         // Tenta receber da fila do joystick (sem bloquear)
//         if (xQueueReceive(xJoystickQueue, &received_joy_dir, 0) == pdTRUE)
//         {
//             strcpy(current_joy_dir, received_joy_dir);
//             screen_needs_update = true;
//         }

//         // Se algum dado novo foi recebido, atualiza a tela
//         if (screen_needs_update)
//         {
//             update_display(current_temp, current_joy_dir);
//         }

//         // Pausa para controlar a taxa de atualização da tela
//         vTaskDelay(pdMS_TO_TICKS(50));
//     }
// }

// --- Função Principal ---
/**
 * @brief Função principal de configuração e inicialização das tarefas.
 */
int main()
{
    stdio_init_all();
    init_oled();
    setup_joystick();
    update_display();

    // Cria as filas para comunicação
    // A fila de temperatura armazena 1 item do tipo 'float'
    xTemperatureQueue = xQueueCreate(1, sizeof(float));
    // A fila do joystick armazena 1 item de 15 bytes (tamanho da string)
    xJoystickQueue = xQueueCreate(1, sizeof(char[15]));

    // Criação das Tarefas
    xTaskCreate(led_task, "LED_Task", 256, NULL, 1, NULL);
    xTaskCreate(temperature_task, "Temp_Task", 256, NULL, 1, NULL);
    xTaskCreate(joystick_task, "Joystick_Task", 256, NULL, 1, NULL);
    // xTaskCreate(update_display, "Display_Task", 512, NULL, 2, NULL);

    // Inicia o escalonador
    vTaskStartScheduler();

    while (1)
    {
    };
}