/* Configuracao para build do MicroSIP/Upphone (Windows, sem video, com G.729 (bcg729) e TLS pelo Schannel do Windows, sem OpenSSL) */
#define PJ_CONFIG_MAXIMUM_SPEED
#include <pj/config_site_sample.h>

#define PJMEDIA_HAS_VIDEO               0
#define PJMEDIA_HAS_OPUS_CODEC          0
#define PJMEDIA_HAS_BCG729              1
#define BCG729_STATIC                   1   /* bcg729 linked as a static library */
#define PJMEDIA_HAS_SRTP                1
#define PJ_HAS_SSL_SOCK                 1
#define PJ_SSL_SOCK_IMP                 PJ_SSL_SOCK_IMP_SCHANNEL
#define PJMEDIA_SRTP_HAS_DTLS           0   /* DTLS-SRTP needs OpenSSL: SRTP keys go in the SDP (SDES), over TLS */

/* config_site_sample.h (PJ_CONFIG_MAXIMUM_SPEED) already sets these two: undef avoids warning C4005 */
#define PJSUA_MAX_ACC                   32
#undef  PJSUA_MAX_CALLS
#define PJSUA_MAX_CALLS                 32
#undef  PJ_IOQUEUE_MAX_HANDLES
#define PJ_IOQUEUE_MAX_HANDLES          1024
