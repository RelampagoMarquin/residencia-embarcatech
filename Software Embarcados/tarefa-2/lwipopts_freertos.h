#ifndef LWIPOPTS_FREERTOS_H
#define LWIPOPTS_FREERTOS_H

// Requerido pelo MQTT
#define LWIP_TCP                    1
#define LWIP_ALTCP                  1
#define LWIP_ALTCP_TLS              0
#define LWIP_IPV4                   1
#define LWIP_IPV6                   0
#define LWIP_NETIF_API              1
#define TCPIP_THREAD_STACKSIZE      1024
#define TCPIP_MBOX_SIZE             8
#define DEFAULT_THREAD_STACKSIZE    1024
#define LWIP_SOCKET                 0
#define LWIP_NETCONN                0

#define MEMP_NUM_TCP_PCB            5
#define MQTT_OUTPUT_RINGBUF_SIZE    512
#define MQTT_VAR_HEADER_BUFFER_LEN  128

#endif // LWIPOPTS_FREERTOS_H
