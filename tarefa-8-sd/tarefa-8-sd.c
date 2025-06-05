#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "inc/ssd1306.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"

// Definição do número de LEDs e pino.
#define LED_COUNT 25

// Definição do I2C para o display OLED
#define I2C_SDA 14
#define I2C_SCL 15

// botoões
#define BTN_B_PIN 6
volatile bool a_state = false; // Botao sw está pressionado?

// variavel do valor
volatile uint8_t value = 0;

#define LOAD_PIN 28 // Pino de LOAD (SH/LD)
#define CLK_PIN 17  // Pino de CLOCK
#define DATA_PIN 16 // Pino de saída serial (Qh)

// função de inicializar o OLED
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

// Função de habilitar as GPIO do CI
void setup_shift_register()
{
    gpio_init(LOAD_PIN);
    gpio_set_dir(LOAD_PIN, GPIO_OUT);
    gpio_put(LOAD_PIN, 1); // Inicia desabilitado

    gpio_init(CLK_PIN);
    gpio_set_dir(CLK_PIN, GPIO_OUT);
    gpio_put(CLK_PIN, 0); // Inicia baixo

    gpio_init(DATA_PIN);
    gpio_set_dir(DATA_PIN, GPIO_IN);
}

// inicializa os botões
void setup_botoes()
{
    // INICIANDO BOTÄO B
    gpio_init(BTN_B_PIN);
    gpio_set_dir(BTN_B_PIN, GPIO_IN);
    gpio_pull_up(BTN_B_PIN);
}

// Fução de exibir no display
void display_value(uint8_t val)
{
    struct render_area frame_area = {0, ssd1306_width - 1, 0, ssd1306_n_pages - 1};
    calculate_render_area_buffer_length(&frame_area);
    uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, ssd1306_buffer_length);

    char buf[16];
    sprintf(buf, "Valor: %u", val);

    ssd1306_draw_string(ssd, 128, 0, buf);
    render_on_display(ssd, &frame_area);
}

// Função de ler os valores
uint8_t read_dip_value()
{
    uint8_t result = 0;

    // Ativa leitura paralela (LOAD baixo)
    gpio_put(LOAD_PIN, 0);
    sleep_us(5); // pulso rápido
    gpio_put(LOAD_PIN, 1);

    // Lê 8 bits, LSB primeiro (chave 8 → 1º bit)
    for (int i = 0; i < 8; i++)
    {
        result |= (gpio_get(DATA_PIN) << i);

        // Pulso de clock
        gpio_put(CLK_PIN, 1);
        sleep_us(2);
        gpio_put(CLK_PIN, 0);
        sleep_us(2);
    }

    return result;
}

int main()
{
    stdio_init_all();
    init_oled();
    setup_shift_register();
    setup_botoes();

    bool btn_anterior = true;

    while (true)
    {
        bool btn_atual = gpio_get(BTN_B_PIN);

        // Detecta borda de descida (pressionado)
        if (!btn_atual && btn_anterior)
        {
            sleep_ms(10); // debounce
            if (!gpio_get(BTN_B_PIN))
            {
                value = read_dip_value();
                display_value(value);
            }
        }

        btn_anterior = btn_atual;
        sleep_ms(500);
    }
}