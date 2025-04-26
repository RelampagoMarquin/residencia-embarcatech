#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "inc/ssd1306.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"

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
void display_menu()
{

    struct render_area frame_area = {0, ssd1306_width - 1, 0, ssd1306_n_pages - 1};
    calculate_render_area_buffer_length(&frame_area);
    uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, ssd1306_buffer_length);

    ssd1306_draw_string(ssd, 5, 0, "    MENU    ");
    for (int i = 0; i < total_options; i++)
    {
        char buffer[16] = {0};
        snprintf(buffer, sizeof(buffer), "%s%s", (i == menu_index) ? "x " : "", menu_options[i]);
        ssd1306_draw_string(ssd, 5, (i + 1) * 18, buffer);
    }
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

    display_menu();
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

void and(){
    if(gpio_get(BTN_A_PIN) && gpio_get(BTN_B_PIN)){
        acende_led_verde();
    }else{
        acende_led_vermelho();
    }
}

void or(){
    if(gpio_get(BTN_A_PIN) || gpio_get(BTN_B_PIN)){
        acende_led_verde();
    }else{
        acende_led_vermelho();
    }
}

void not(){
    // 
    if(gpio_get(BTN_A_PIN)){
        acende_led_verde();
    }else{
        acende_led_vermelho();
    }
}

void nand(){
    if(!gpio_get(BTN_A_PIN) || !gpio_get(BTN_B_PIN)){
        acende_led_verde();
    }else{
        acende_led_vermelho();
    }
}

void nor(){
    if(!gpio_get(BTN_A_PIN) && !gpio_get(BTN_B_PIN)){
        acende_led_verde();
    }else{
        acende_led_vermelho();
    }
}

void xor(){
    if((gpio_get(BTN_A_PIN) && !gpio_get(BTN_B_PIN)) || (!gpio_get(BTN_A_PIN) && gpio_get(BTN_B_PIN))){
        acende_led_verde();
    }else{
        acende_led_vermelho();
    }
}

void xnor(){
    if((!gpio_get(BTN_A_PIN) && !gpio_get(BTN_B_PIN)) || (gpio_get(BTN_A_PIN) && gpio_get(BTN_B_PIN))){
        acende_led_verde();
    }else{
        acende_led_vermelho();
    }
}

void portas_logicas(int menu_index)
{
    switch (menu_index)
    {
    case 0:
        and();
        break;
    case 1:
        or();
        break;
    case 2:
        not();
        break;
    case 3:
        nand();
        break;
    case 4:
        nor();
        break;
    case 5:
        xor();
        break;
    case 6:
        xnor();
        break;
    default:
        break;
    }
}



int main()
{
    setup_joystick();
    init_oled();
    setup_botoes();
    setup_led();
    display_menu();

    while (true)
    {
        navigate_menu();
        portas_logicas(menu_index);
    }
    
    sleep_ms(500);
}