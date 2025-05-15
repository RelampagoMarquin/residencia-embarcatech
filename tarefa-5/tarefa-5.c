#include <stdio.h> 
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/pio.h"

// Definição dos pinos do LED
#define LED_R_PIN 13
#define LED_G_PIN 11
#define LED_B_PIN 12

// Pino do botão A
#define BTN_A_PIN 5

const char *menu_options[] = {"R", "G", "B"};

void setup_led()
{
    gpio_init(LED_R_PIN);
    gpio_set_dir(LED_R_PIN, GPIO_OUT);
    gpio_init(LED_G_PIN);
    gpio_set_dir(LED_G_PIN, GPIO_OUT);
    gpio_init(LED_B_PIN);
    gpio_set_dir(LED_B_PIN, GPIO_OUT);
}

void setup_botoes()
{
    gpio_init(BTN_A_PIN);
    gpio_set_dir(BTN_A_PIN, GPIO_IN);
    gpio_pull_up(BTN_A_PIN);
}

void muda_cor(int position)
{
    switch (position)
    {
    case 0: // Vermelho
        gpio_put(LED_R_PIN, 1);
        gpio_put(LED_G_PIN, 0);
        gpio_put(LED_B_PIN, 0);
        break;
    case 1: // Verde
        gpio_put(LED_R_PIN, 0);
        gpio_put(LED_G_PIN, 1);
        gpio_put(LED_B_PIN, 0);
        break;
    case 2: // Azul
        gpio_put(LED_R_PIN, 0);
        gpio_put(LED_G_PIN, 0);
        gpio_put(LED_B_PIN, 1);
        break;
    default:
        break;
    }
}

int main()
{
    stdio_init_all();

    setup_led();
    setup_botoes();

    int position = 0;
    int last_button_state = 1;

    muda_cor(position); // Cor inicial

    while (true)
    {
        int button_state = gpio_get(BTN_A_PIN);

        if (button_state == 0 && last_button_state == 1)
        {
            // Avança a posição com ciclo
            position = (position + 1) % 3;
            muda_cor(position);
            sleep_ms(300); // debounce
        }

        last_button_state = button_state;
        sleep_ms(200); // pequeno delay para evitar uso excessivo da CPU
    }
}
