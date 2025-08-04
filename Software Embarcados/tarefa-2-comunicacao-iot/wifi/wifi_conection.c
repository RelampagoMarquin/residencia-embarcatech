#include "wifi_conection.h"
#include "pico/cyw43_arch.h"
#include "lwip/netif.h"
#include "lwip/ip4_addr.h"

bool is_wifi_connected() {
    int link_status = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);

    // A conexão é considerada ativa se o status for:
    // CYW43_LINK_UP: Conectado e com endereço IP.
    return link_status == CYW43_LINK_UP;
}
  
// Função para conectar ao Wi-Fi
int conectar_wifi(uint8_t width, uint8_t n_pages, int buffer_length)
{

    struct render_area frame_area = {0, width - 1, 0, n_pages - 1};

    // Inicializa o Wi-Fi
    if (cyw43_arch_init())
    {
        calculate_render_area_buffer_length(&frame_area);
        uint8_t ssd[buffer_length];
        memset(ssd, 0, buffer_length);
        ssd1306_draw_string(ssd, 5, 20, "Erro ao iniciar");
        ssd1306_draw_string(ssd, 5, 40, "   o Wi-Fi");
        render_on_display(ssd, &frame_area);
        sleep_ms(2000);
        return 1;
    }

    cyw43_arch_enable_sta_mode();
    calculate_render_area_buffer_length(&frame_area);
    uint8_t ssd[buffer_length];
    memset(ssd, 0, buffer_length);
    ssd1306_draw_string(ssd, 5, 20, "Conectando WiFi");
    ssd1306_draw_string(ssd, 5, 40, "Aguarde...");
    render_on_display(ssd, &frame_area);
    sleep_ms(2000);

    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASS, CYW43_AUTH_WPA2_AES_PSK, 20000))
    {
        calculate_render_area_buffer_length(&frame_area);
        uint8_t ssd[buffer_length];
        memset(ssd, 0, buffer_length);
        ssd1306_draw_string(ssd, 5, 20, "Falha ao");
        ssd1306_draw_string(ssd, 5, 30, "conectar");
        render_on_display(ssd, &frame_area);
        sleep_ms(2000);
        return 1;
    }
    else
    {
        calculate_render_area_buffer_length(&frame_area);
        uint8_t ssd[buffer_length];
        memset(ssd, 0, buffer_length);
        ssd1306_draw_string(ssd, 5, 10, "Conectado");
        ssd1306_draw_string(ssd, 5, 24, "Endereco IP ");

        // obtendo o endereço IP usando a API do LwIP
        struct netif *netif = netif_default;
        const ip4_addr_t *ip_addr = netif_ip4_addr(netif);
        
        char buffer[22] = {0};

        // Função 'ip4addr_ntoa' converte o IP para uma string (ex: "192.168.1.5")
        snprintf(buffer, sizeof(buffer), "%s", ip4addr_ntoa(ip_addr));
        ssd1306_draw_string(ssd, 5, 32, buffer);
        render_on_display(ssd, &frame_area);
        sleep_ms(5000);
        return 0;
    }
}