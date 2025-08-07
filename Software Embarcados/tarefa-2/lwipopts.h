#ifndef _LWIPOPTS_H
#define _LWIPOPTS_H

// ===================================================================
// PARTE 1: CONFIGURAÇÕES GERAIS E PARA FREERTOS
// ===================================================================

// Queremos usar o LwIP com um Sistema Operacional (FreeRTOS)
#define NO_SYS                      0
// Habilita a proteção de threads, essencial para FreeRTOS
#define SYS_LIGHTWEIGHT_PROT        1
// Desativa o mapeamento de erros para o padrão 'errno' do sistema
#define LWIP_PROVIDE_ERRNO          0

// --- Configurações de Memória ---
// Deixa o LwIP gerenciar seus próprios pools de memória, mais seguro com RTOS
#define MEM_LIBC_MALLOC             0
#define MEMP_MEM_MALLOC             0 // Também usa os pools internos
#define MEM_ALIGNMENT               4
#define MEM_SIZE                    (12 * 1024)

// --- Configurações de Rede ---
#define PICO_CYW43_ARCH_LWIP_IPV4   1
#define PICO_CYW43_ARCH_LWIP_IPV6   0
#define LWIP_ETHERNET               1
#define LWIP_ICMP                   1
#define LWIP_DNS                    1
#define LWIP_TCP                    1
#define LWIP_UDP                    1
#define LWIP_TCP_MSS                1460
#define LWIP_TCP_WND                (8 * LWIP_TCP_MSS)
#define LWIP_TCP_SND_BUF            (8 * LWIP_TCP_MSS)
#define LWIP_TCP_SND_QUEUELEN       ((4 * (LWIP_TCP_SND_BUF) + (LWIP_TCP_MSS - 1)) / (LWIP_TCP_MSS))
#define LWIP_NETIF_HOSTNAME         1
#define LWIP_NETIF_STATUS_CALLBACK  1
#define LWIP_NETIF_LINK_CALLBACK    1
#define LWIP_NETCONN                0
#define LWIP_SOCKET                 0

// --- Configurações de DHCP e DNS ---
#define LWIP_DHCP                   1
#define LWIP_DNS_SUPPORT_MDNS_QUERIES 1

// --- Configurações de Checagem e Debug ---
#define LWIP_CHECKSUM_CTRL_PER_NETIF 1
#define CHECKSUM_GEN_TCP            (PICO_CYW43_ARCH_OPTIMIZE_TCP_CHECKSUM ? 0 : 1)
#define CHECKSUM_GEN_UDP            (PICO_CYW43_ARCH_OPTIMIZE_TCP_CHECKSUM ? 0 : 1)
#define CHECKSUM_CHECK_TCP          (PICO_CYW43_ARCH_OPTIMIZE_TCP_CHECKSUM ? 0 : 1)
#define CHECKSUM_CHECK_UDP          (PICO_CYW43_ARCH_OPTIMIZE_TCP_CHECKSUM ? 0 : 1)
#define CHECKSUM_GEN_IP             (PICO_CYW43_ARCH_OPTIMIZE_TCP_CHECKSUM ? 0 : 1)
#define CHECKSUM_CHECK_IP           (PICO_CYW43_ARCH_OPTIMIZE_TCP_CHECKSUM ? 0 : 1)

// ===================================================================
// PARTE 2: CONTEÚDO MESCLADO DE "lwipopts_freertos.h"
// (Isto substitui a linha #include que estava a falhar)
// ===================================================================

// Requerido pelo MQTT e FreeRTOS
#define LWIP_ALTCP                  1
#define LWIP_ALTCP_TLS              0
#define LWIP_NETIF_API              1
#define TCPIP_THREAD_STACKSIZE      1024
#define TCPIP_MBOX_SIZE             8
#define DEFAULT_THREAD_STACKSIZE    1024
#define MEMP_NUM_TCP_PCB            5
#define MQTT_OUTPUT_RINGBUF_SIZE    512
#define MQTT_VAR_HEADER_BUFFER_LEN  128

#endif /* _LWIPOPTS_H */