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
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"

// Biblioteca gerada pelo arquivo .pio durante compilação.
#include "ws2818b.pio.h"

// Definição do número de LEDs e pino.
#define LED_COUNT 25
#define LED_PIN 7


// Definição dos pinos do joystick e do botão
#define VRX 26
#define VRY 27
#define SW 22

// Definição dos pinos do LED
#define LED_R_PIN 13
#define LED_G_PIN 11
#define LED_B_PIN 12

// Definição do I2C para o display OLED
#define I2C_SDA 14
#define I2C_SCL 15

// botoões
// pino do botão A
#define BTN_A_PIN 5
#define BTN_B_PIN 6

volatile bool a_state = false; // Botao A está pressionado?
volatile bool b_state = false; // Botao A está pressionado?

// variaveis do menu
const char *menu_options[] = {"AND", "OR", "NOT", "NAND", "NOR", "XOR", "XNOR"};
int menu_index = 0;
int total_options = 7;
volatile int no_menu = 1;

// inicializa o joystick
void setup_joystick()
{
    adc_init();
    adc_gpio_init(VRX);
    adc_gpio_init(VRY);
    gpio_init(SW);
    gpio_set_dir(SW, GPIO_IN);
    gpio_pull_up(SW);
}

// inicializa o led
void setup_led()
{
    gpio_init(LED_R_PIN);
    gpio_set_dir(LED_R_PIN, GPIO_OUT);
    gpio_init(LED_G_PIN);
    gpio_set_dir(LED_G_PIN, GPIO_OUT);
    gpio_init(LED_B_PIN);
    gpio_set_dir(LED_B_PIN, GPIO_OUT);
}

// inicializa os botões
void setup_botoes()
{
    // INICIANDO BOTÄO A
    gpio_init(BTN_A_PIN);
    gpio_set_dir(BTN_A_PIN, GPIO_IN);
    gpio_pull_up(BTN_A_PIN);

    // INICIANDO BOTÄO B
    gpio_init(BTN_B_PIN);
    gpio_set_dir(BTN_B_PIN, GPIO_IN);
    gpio_pull_up(BTN_B_PIN);
}

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

// função de leitura do joystick
void joystick_read_axis(uint16_t *vrx_value, uint16_t *vry_value)
{
    adc_select_input(0);
    sleep_us(2);
    *vry_value = adc_read();

    adc_select_input(1);
    sleep_us(2);
    *vrx_value = adc_read();
}

// Função de exibir o menu
void display_menu(int selected_option)
{

    struct render_area frame_area = {0, ssd1306_width - 1, 0, ssd1306_n_pages - 1};
    calculate_render_area_buffer_length(&frame_area);
    uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, ssd1306_buffer_length);

    const char* options[] = {"AND", "OR", "NOT", "NAND", "NOR", "XOR", "XNOR"};
    const char* current_option = options[selected_option];

    int text_width = strlen(current_option) * 6;  // Cada caractere tem 6 pixels de largura (fonte padrão 6x8)
    int x = (128 - text_width) / 2;                // Centraliza no eixo X
    int y = (64 - 8) / 2;                          // Centraliza no eixo Y (altura do texto é 8px)

    ssd1306_draw_string(ssd, x, y, (char*) current_option);     
    
    render_on_display(ssd, &frame_area);
}

// Navegação do menu com joystick
void navigate_menu()
{
    uint16_t vrx_value, vry_value;
    joystick_read_axis(&vrx_value, &vry_value);

    if (vry_value > 3000)
    {
        menu_index = (menu_index - 1 + total_options) % total_options; // Cima
    }
    else if (vry_value < 1000)
    {
        menu_index = (menu_index + 1) % total_options; // Baixo
    }

    display_menu(menu_index);
    sleep_ms(300);
}

void acende_led_verde()
{
    gpio_put(LED_G_PIN, 1);
    gpio_put(LED_R_PIN, 0);
    gpio_put(LED_B_PIN, 0);
}

void acende_led_vermelho()
{
    gpio_put(LED_G_PIN, 0);
    gpio_put(LED_R_PIN, 1);
    gpio_put(LED_B_PIN, 0);
}

void espera_com_leitura(int timeMS)
{
    for (int i = 0; i < timeMS; i += 100)
    {
        if (!gpio_get(BTN_A_PIN)) // Verifica se o botão foi pressionado
        {
            a_state = false;
        }
        sleep_ms(100);
    }
}

// Definição de pixel GRB
struct pixel_t {
 uint8_t G, R, B; // Três valores de 8-bits compõem um pixel.
};
typedef struct pixel_t pixel_t;
typedef pixel_t npLED_t; // Mudança de nome de "struct pixel_t" para "npLED_t" por clareza.

// Declaração do buffer de pixels que formam a matriz.
npLED_t leds[LED_COUNT];

// Variáveis para uso da máquina PIO.
PIO np_pio;
uint sm;

/**
* Inicializa a máquina PIO para controle da matriz de LEDs.
*/
void npInit(uint pin) {

 // Cria programa PIO.
 uint offset = pio_add_program(pio0, &ws2818b_program);
 np_pio = pio0;

 // Toma posse de uma máquina PIO.
 sm = pio_claim_unused_sm(np_pio, false);
 if (sm < 0) {
   np_pio = pio1;
   sm = pio_claim_unused_sm(np_pio, true); // Se nenhuma máquina estiver livre, panic!
 }

 // Inicia programa na máquina PIO obtida.
 ws2818b_program_init(np_pio, sm, offset, pin, 800000.f);

 // Limpa buffer de pixels.
 for (uint i = 0; i < LED_COUNT; ++i) {
   leds[i].R = 0;
   leds[i].G = 0;
   leds[i].B = 0;
 }
}

/**
* Atribui uma cor RGB a um LED.
*/
void npSetLED(const uint index, const uint8_t r, const uint8_t g, const uint8_t b) {
 leds[index].R = r;
 leds[index].G = g;
 leds[index].B = b;
}

/**
* Limpa o buffer de pixels.
*/
void npClear() {
 for (uint i = 0; i < LED_COUNT; ++i)
   npSetLED(i, 0, 0, 0);
}

/**
* Escreve os dados do buffer nos LEDs.
*/
void npWrite() {
 // Escreve cada dado de 8-bits dos pixels em sequência no buffer da máquina PIO.
 for (uint i = 0; i < LED_COUNT; ++i) {
   pio_sm_put_blocking(np_pio, sm, leds[i].G);
   pio_sm_put_blocking(np_pio, sm, leds[i].R);
   pio_sm_put_blocking(np_pio, sm, leds[i].B);
 }
}


// int main()
// {
//     setup_joystick();
//     init_oled();
//     setup_botoes();
//     setup_led();
//     display_menu(menu_index);

//     while (true)
//     {
//         navigate_menu();
//         portas_logicas(menu_index);
//     }
    
//     sleep_ms(500);
// }

int main() {

 // Inicializa entradas e saídas.
 stdio_init_all();

 // Inicializa matriz de LEDs NeoPixel.
 npInit(LED_PIN);
 npClear();

 // Aqui, você desenha nos LEDs.
 npSetLED(4, 255, 0, 0); // Define o LED de índice 0 para vermelho.
 npSetLED(3, 255, 0, 0); // Define o LED de índice 0 para vermelho.
 npSetLED(2, 255, 0, 0); // Define o LED de índice 12 (centro da matriz) para verde.
 npSetLED(1, 0, 255, 0); // Define o LED de índice 0 para vermelho.

 npWrite(); // Escreve os dados nos LEDs.

 // Não faz mais nada. Loop infinito.
 while (true) {
   sleep_ms(1000);
 }
}