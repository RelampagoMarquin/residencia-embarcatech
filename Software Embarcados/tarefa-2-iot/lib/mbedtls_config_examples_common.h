#ifndef MBEDTLS_CONFIG_EXAMPLES_COMMON_H
#define MBEDTLS_CONFIG_EXAMPLES_COMMON_H

/* Workaround for some mbedtls source files using INT_MAX without including limits.h */
#include <limits.h>

// ===================================================================
// CONFIGURAÇÃO MÍNIMA SEM TLS
// Apenas as definições básicas de plataforma e tempo são mantidas
// para garantir a compatibilidade com o SDK do Pico.
// ===================================================================

// --- Definições da Plataforma Pico ---
#define MBEDTLS_PLATFORM_C
#define MBEDTLS_NO_PLATFORM_ENTROPY
#define MBEDTLS_ENTROPY_HARDWARE_ALT
#define MBEDTLS_HAVE_TIME
#define MBEDTLS_TIMING_C
#define MBEDTLS_TIMING_ALT

// --- Módulos Essenciais Mínimos ---
#define MBEDTLS_ERROR_C      // Necessário para a gestão de erros

/* Todas as outras definições relacionadas com SSL, TLS, X509 (certificados),
   PEM, Base64, e algoritmos de criptografia complexos (RSA, ECP, etc.)
   foram removidas, pois não são necessárias sem TLS.
*/

#endif /* MBEDTLS_CONFIG_EXAMPLES_COMMON_H */
