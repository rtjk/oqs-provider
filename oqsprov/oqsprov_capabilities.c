// SPDX-License-Identifier: Apache-2.0 AND MIT

/*
 * OQS OpenSSL 3 provider
 *
 * Code strongly inspired by OpenSSL common provider capabilities.
 *
 * ToDo: Interop testing.
 */

#include <assert.h>
#include <openssl/core_dispatch.h>
#include <openssl/core_names.h>
#include <string.h>

/* For TLS1_VERSION etc */
#include <openssl/params.h>
#include <openssl/ssl.h>

// internal, but useful OSSL define:
#define OSSL_NELEM(x) (sizeof(x) / sizeof((x)[0]))

// enables DTLS1.3 testing even before available in openssl master:
#if !defined(DTLS1_3_VERSION)
#define DTLS1_3_VERSION 0xFEFC
#endif

#include "oqs_prov.h"

typedef struct oqs_group_constants_st {
    unsigned int group_id; /* Group ID */
    unsigned int secbits;  /* Bits of security */
    int mintls;            /* Minimum TLS version, -1 unsupported */
    int maxtls;            /* Maximum TLS version (or 0 for undefined) */
    int mindtls;           /* Minimum DTLS version, -1 unsupported */
    int maxdtls;           /* Maximum DTLS version (or 0 for undefined) */
    int is_kem;            /* Always set */
} OQS_GROUP_CONSTANTS;

static OQS_GROUP_CONSTANTS oqs_group_list[] = {
    // ad-hoc assignments - take from OQS generate data structures
    ///// OQS_TEMPLATE_FRAGMENT_GROUP_ASSIGNMENTS_START
   { 65024, 128, TLS1_3_VERSION, 0, DTLS1_3_VERSION, 0, 1 },

   { 65025, 128, TLS1_3_VERSION, 0, DTLS1_3_VERSION, 0, 1 },
   { 65026, 128, TLS1_3_VERSION, 0, DTLS1_3_VERSION, 0, 1 },
   { 65027, 128, TLS1_3_VERSION, 0, DTLS1_3_VERSION, 0, 1 },

   { 65028, 128, TLS1_3_VERSION, 0, DTLS1_3_VERSION, 0, 1 },
   { 65029, 128, TLS1_3_VERSION, 0, DTLS1_3_VERSION, 0, 1 },
   { 65030, 192, TLS1_3_VERSION, 0, DTLS1_3_VERSION, 0, 1 },

   { 65031, 192, TLS1_3_VERSION, 0, DTLS1_3_VERSION, 0, 1 },
   { 65032, 192, TLS1_3_VERSION, 0, DTLS1_3_VERSION, 0, 1 },
   { 65033, 192, TLS1_3_VERSION, 0, DTLS1_3_VERSION, 0, 1 },

   { 65034, 192, TLS1_3_VERSION, 0, DTLS1_3_VERSION, 0, 1 },
   { 65035, 192, TLS1_3_VERSION, 0, DTLS1_3_VERSION, 0, 1 },
   { 65036, 256, TLS1_3_VERSION, 0, DTLS1_3_VERSION, 0, 1 },

   { 65037, 256, TLS1_3_VERSION, 0, DTLS1_3_VERSION, 0, 1 },
   { 65038, 256, TLS1_3_VERSION, 0, DTLS1_3_VERSION, 0, 1 },

   { 65039, 256, TLS1_3_VERSION, 0, DTLS1_3_VERSION, 0, 1 },
   { 512, 128, TLS1_3_VERSION, 0, DTLS1_3_VERSION, 0, 1 },

   { 0x2F4B, 128, TLS1_3_VERSION, 0, DTLS1_3_VERSION, 0, 1 },
   { 0x2FB6, 128, TLS1_3_VERSION, 0, DTLS1_3_VERSION, 0, 1 },
   { 513, 192, TLS1_3_VERSION, 0, DTLS1_3_VERSION, 0, 1 },

   { 0x2F4C, 192, TLS1_3_VERSION, 0, DTLS1_3_VERSION, 0, 1 },
   { 0x2FB7, 192, TLS1_3_VERSION, 0, DTLS1_3_VERSION, 0, 1 },
   { 0x11ec, 192, TLS1_3_VERSION, 0, DTLS1_3_VERSION, 0, 1 },
   { 0x11eb, 192, TLS1_3_VERSION, 0, DTLS1_3_VERSION, 0, 1 },
   { 514, 256, TLS1_3_VERSION, 0, DTLS1_3_VERSION, 0, 1 },

   { 0x2F4D, 256, TLS1_3_VERSION, 0, DTLS1_3_VERSION, 0, 1 },
   { 0x11ED, 256, TLS1_3_VERSION, 0, DTLS1_3_VERSION, 0, 1 },
   { 65040, 128, TLS1_3_VERSION, 0, DTLS1_3_VERSION, 0, 1 },

   { 65041, 128, TLS1_3_VERSION, 0, DTLS1_3_VERSION, 0, 1 },
   { 65042, 128, TLS1_3_VERSION, 0, DTLS1_3_VERSION, 0, 1 },
   { 65043, 192, TLS1_3_VERSION, 0, DTLS1_3_VERSION, 0, 1 },

   { 65044, 192, TLS1_3_VERSION, 0, DTLS1_3_VERSION, 0, 1 },
   { 65045, 192, TLS1_3_VERSION, 0, DTLS1_3_VERSION, 0, 1 },
   { 65046, 256, TLS1_3_VERSION, 0, DTLS1_3_VERSION, 0, 1 },

   { 65047, 256, TLS1_3_VERSION, 0, DTLS1_3_VERSION, 0, 1 },
///// OQS_TEMPLATE_FRAGMENT_GROUP_ASSIGNMENTS_END
};

// Adds entries for tlsname, `ecx`_tlsname and `ecp`_tlsname
#define OQS_GROUP_ENTRY(tlsname, realname, algorithm, idx)                     \
    {                                                                          \
        OSSL_PARAM_utf8_string(OSSL_CAPABILITY_TLS_GROUP_NAME, #tlsname,       \
                               sizeof(#tlsname)),                              \
            OSSL_PARAM_utf8_string(OSSL_CAPABILITY_TLS_GROUP_NAME_INTERNAL,    \
                                   #realname, sizeof(#realname)),              \
            OSSL_PARAM_utf8_string(OSSL_CAPABILITY_TLS_GROUP_ALG, #algorithm,  \
                                   sizeof(#algorithm)),                        \
            OSSL_PARAM_uint(OSSL_CAPABILITY_TLS_GROUP_ID,                      \
                            (unsigned int *)&oqs_group_list[idx].group_id),    \
            OSSL_PARAM_uint(OSSL_CAPABILITY_TLS_GROUP_SECURITY_BITS,           \
                            (unsigned int *)&oqs_group_list[idx].secbits),     \
            OSSL_PARAM_int(OSSL_CAPABILITY_TLS_GROUP_MIN_TLS,                  \
                           (unsigned int *)&oqs_group_list[idx].mintls),       \
            OSSL_PARAM_int(OSSL_CAPABILITY_TLS_GROUP_MAX_TLS,                  \
                           (unsigned int *)&oqs_group_list[idx].maxtls),       \
            OSSL_PARAM_int(OSSL_CAPABILITY_TLS_GROUP_MIN_DTLS,                 \
                           (unsigned int *)&oqs_group_list[idx].mindtls),      \
            OSSL_PARAM_int(OSSL_CAPABILITY_TLS_GROUP_MAX_DTLS,                 \
                           (unsigned int *)&oqs_group_list[idx].maxdtls),      \
            OSSL_PARAM_int(OSSL_CAPABILITY_TLS_GROUP_IS_KEM,                   \
                           (unsigned int *)&oqs_group_list[idx].is_kem),       \
            OSSL_PARAM_END                                                     \
    }

static const OSSL_PARAM oqs_param_group_list[][11] = {
///// OQS_TEMPLATE_FRAGMENT_GROUP_NAMES_START

#ifdef OQS_ENABLE_KEM_frodokem_640_aes
    OQS_GROUP_ENTRY(frodo640aes, frodo640aes, frodo640aes, 0),

    OQS_GROUP_ENTRY(p256_frodo640aes, p256_frodo640aes, p256_frodo640aes, 1),
    OQS_GROUP_ENTRY(x25519_frodo640aes, x25519_frodo640aes, x25519_frodo640aes, 2),
#endif
#ifdef OQS_ENABLE_KEM_frodokem_640_shake
    OQS_GROUP_ENTRY(frodo640shake, frodo640shake, frodo640shake, 3),

    OQS_GROUP_ENTRY(p256_frodo640shake, p256_frodo640shake, p256_frodo640shake, 4),
    OQS_GROUP_ENTRY(x25519_frodo640shake, x25519_frodo640shake, x25519_frodo640shake, 5),
#endif
#ifdef OQS_ENABLE_KEM_frodokem_976_aes
    OQS_GROUP_ENTRY(frodo976aes, frodo976aes, frodo976aes, 6),

    OQS_GROUP_ENTRY(p384_frodo976aes, p384_frodo976aes, p384_frodo976aes, 7),
    OQS_GROUP_ENTRY(x448_frodo976aes, x448_frodo976aes, x448_frodo976aes, 8),
#endif
#ifdef OQS_ENABLE_KEM_frodokem_976_shake
    OQS_GROUP_ENTRY(frodo976shake, frodo976shake, frodo976shake, 9),

    OQS_GROUP_ENTRY(p384_frodo976shake, p384_frodo976shake, p384_frodo976shake, 10),
    OQS_GROUP_ENTRY(x448_frodo976shake, x448_frodo976shake, x448_frodo976shake, 11),
#endif
#ifdef OQS_ENABLE_KEM_frodokem_1344_aes
    OQS_GROUP_ENTRY(frodo1344aes, frodo1344aes, frodo1344aes, 12),

    OQS_GROUP_ENTRY(p521_frodo1344aes, p521_frodo1344aes, p521_frodo1344aes, 13),
#endif
#ifdef OQS_ENABLE_KEM_frodokem_1344_shake
    OQS_GROUP_ENTRY(frodo1344shake, frodo1344shake, frodo1344shake, 14),

    OQS_GROUP_ENTRY(p521_frodo1344shake, p521_frodo1344shake, p521_frodo1344shake, 15),
#endif
#ifdef OQS_ENABLE_KEM_ml_kem_512
    OQS_GROUP_ENTRY(mlkem512, mlkem512, mlkem512, 16),

    OQS_GROUP_ENTRY(p256_mlkem512, p256_mlkem512, p256_mlkem512, 17),
    OQS_GROUP_ENTRY(x25519_mlkem512, x25519_mlkem512, x25519_mlkem512, 18),
#endif
#ifdef OQS_ENABLE_KEM_ml_kem_768
    OQS_GROUP_ENTRY(mlkem768, mlkem768, mlkem768, 19),

    OQS_GROUP_ENTRY(p384_mlkem768, p384_mlkem768, p384_mlkem768, 20),
    OQS_GROUP_ENTRY(x448_mlkem768, x448_mlkem768, x448_mlkem768, 21),
    OQS_GROUP_ENTRY(X25519MLKEM768, X25519MLKEM768, X25519MLKEM768, 22),
    OQS_GROUP_ENTRY(SecP256r1MLKEM768, SecP256r1MLKEM768, SecP256r1MLKEM768, 23),
#endif
#ifdef OQS_ENABLE_KEM_ml_kem_1024
    OQS_GROUP_ENTRY(mlkem1024, mlkem1024, mlkem1024, 24),

    OQS_GROUP_ENTRY(p521_mlkem1024, p521_mlkem1024, p521_mlkem1024, 25),
    OQS_GROUP_ENTRY(SecP384r1MLKEM1024, SecP384r1MLKEM1024, SecP384r1MLKEM1024, 26),
#endif
#ifdef OQS_ENABLE_KEM_bike_l1
    OQS_GROUP_ENTRY(bikel1, bikel1, bikel1, 27),

    OQS_GROUP_ENTRY(p256_bikel1, p256_bikel1, p256_bikel1, 28),
    OQS_GROUP_ENTRY(x25519_bikel1, x25519_bikel1, x25519_bikel1, 29),
#endif
#ifdef OQS_ENABLE_KEM_bike_l3
    OQS_GROUP_ENTRY(bikel3, bikel3, bikel3, 30),

    OQS_GROUP_ENTRY(p384_bikel3, p384_bikel3, p384_bikel3, 31),
    OQS_GROUP_ENTRY(x448_bikel3, x448_bikel3, x448_bikel3, 32),
#endif
#ifdef OQS_ENABLE_KEM_bike_l5
    OQS_GROUP_ENTRY(bikel5, bikel5, bikel5, 33),

    OQS_GROUP_ENTRY(p521_bikel5, p521_bikel5, p521_bikel5, 34),
#endif
///// OQS_TEMPLATE_FRAGMENT_GROUP_NAMES_END
};

typedef struct oqs_sigalg_constants_st {
    unsigned int code_point; /* Code point */
    unsigned int secbits;    /* Bits of security */
    int mintls;              /* Minimum TLS version, -1 unsupported */
    int maxtls;              /* Maximum TLS version (or 0 for undefined) */
} OQS_SIGALG_CONSTANTS;

static OQS_SIGALG_CONSTANTS oqs_sigalg_list[] = {
    // ad-hoc assignments - take from OQS generate data structures
    ///// OQS_TEMPLATE_FRAGMENT_SIGALG_ASSIGNMENTS_START
    { 0x0904, 128, TLS1_3_VERSION, 0 },
    { 0xff06, 128, TLS1_3_VERSION, 0 },
    { 0xff07, 128, TLS1_3_VERSION, 0 },
    { 0x090f, 128, TLS1_3_VERSION, 0 },
    { 0x090c, 128, TLS1_3_VERSION, 0 },
    { 0x090a, 128, TLS1_3_VERSION, 0 },
    { 0x0907, 128, TLS1_3_VERSION, 0 },
    { 0xfee5, 128, TLS1_3_VERSION, 0 },
    { 0x0905, 192, TLS1_3_VERSION, 0 },
    { 0xff08, 192, TLS1_3_VERSION, 0 },
    { 0x0910, 192, TLS1_3_VERSION, 0 },
    { 0x090d, 192, TLS1_3_VERSION, 0 },
    { 0x0908, 192, TLS1_3_VERSION, 0 },
    { 0xfee9, 192, TLS1_3_VERSION, 0 },
    { 0x090b, 192, TLS1_3_VERSION, 0 },
    { 0x0906, 256, TLS1_3_VERSION, 0 },
    { 0xff09, 256, TLS1_3_VERSION, 0 },
    { 0x0909, 256, TLS1_3_VERSION, 0 },
    { 0xfeec, 256, TLS1_3_VERSION, 0 },
    { 0x0912, 256, TLS1_3_VERSION, 0 },
    { 0xfed7, 128, TLS1_3_VERSION, 0 },
    { 0xfed8, 128, TLS1_3_VERSION, 0 },
    { 0xfed9, 128, TLS1_3_VERSION, 0 },
    { 0xfedc, 128, TLS1_3_VERSION, 0 },
    { 0xfedd, 128, TLS1_3_VERSION, 0 },
    { 0xfede, 128, TLS1_3_VERSION, 0 },
    { 0xfeda, 256, TLS1_3_VERSION, 0 },
    { 0xfedb, 256, TLS1_3_VERSION, 0 },
    { 0xfedf, 256, TLS1_3_VERSION, 0 },
    { 0xfee0, 256, TLS1_3_VERSION, 0 },
    { 0xfeb3, 128, TLS1_3_VERSION, 0 },
    { 0xfeb4, 128, TLS1_3_VERSION, 0 },
    { 0xfeb5, 128, TLS1_3_VERSION, 0 },
    { 0xfeb6, 128, TLS1_3_VERSION, 0 },
    { 0xfeb7, 128, TLS1_3_VERSION, 0 },
    { 0xfeb8, 128, TLS1_3_VERSION, 0 },
    { 0xfeb9, 192, TLS1_3_VERSION, 0 },
    { 0xfeba, 192, TLS1_3_VERSION, 0 },
    { 0xfebb, 192, TLS1_3_VERSION, 0 },
    { 0xfebc, 192, TLS1_3_VERSION, 0 },
    { 0xfebd, 256, TLS1_3_VERSION, 0 },
    { 0xfebe, 256, TLS1_3_VERSION, 0 },
    { 0xfec0, 256, TLS1_3_VERSION, 0 },
    { 0xfec1, 256, TLS1_3_VERSION, 0 },
    { 0xfec2, 128, TLS1_3_VERSION, 0 },
    { 0xfec3, 128, TLS1_3_VERSION, 0 },
    { 0xfec4, 128, TLS1_3_VERSION, 0 },
    { 0xfec5, 128, TLS1_3_VERSION, 0 },
    { 0xfec6, 128, TLS1_3_VERSION, 0 },
    { 0xfec7, 128, TLS1_3_VERSION, 0 },
    { 0xfec8, 192, TLS1_3_VERSION, 0 },
    { 0xfec9, 192, TLS1_3_VERSION, 0 },
    { 0xfeca, 192, TLS1_3_VERSION, 0 },
    { 0xfecb, 192, TLS1_3_VERSION, 0 },
    { 0xfecc, 256, TLS1_3_VERSION, 0 },
    { 0xfecd, 256, TLS1_3_VERSION, 0 },
    { 0xfece, 256, TLS1_3_VERSION, 0 },
    { 0xfecf, 256, TLS1_3_VERSION, 0 },
    { 0xff32, 128, TLS1_3_VERSION, 0 },
    { 0xff36, 128, TLS1_3_VERSION, 0 },
    { 0xff33, 128, TLS1_3_VERSION, 0 },
    { 0xff37, 128, TLS1_3_VERSION, 0 },
    { 0xff34, 192, TLS1_3_VERSION, 0 },
    { 0xff38, 192, TLS1_3_VERSION, 0 },
    { 0xff35, 256, TLS1_3_VERSION, 0 },
    { 0xff39, 256, TLS1_3_VERSION, 0 },
    { 0xff22, 128, TLS1_3_VERSION, 0 },
    { 0xff23, 128, TLS1_3_VERSION, 0 },
    { 0xff24, 128, TLS1_3_VERSION, 0 },
    { 0xff25, 192, TLS1_3_VERSION, 0 },
    { 0xff26, 192, TLS1_3_VERSION, 0 },
    { 0xff27, 192, TLS1_3_VERSION, 0 },
    { 0xff53, 256, TLS1_3_VERSION, 0 },
    { 0xff54, 256, TLS1_3_VERSION, 0 },
    { 0xff28, 256, TLS1_3_VERSION, 0 },
    { 0xff29, 128, TLS1_3_VERSION, 0 },
    { 0xff2a, 128, TLS1_3_VERSION, 0 },
    { 0xff2b, 128, TLS1_3_VERSION, 0 },
    { 0xff2c, 192, TLS1_3_VERSION, 0 },
    { 0xff2d, 192, TLS1_3_VERSION, 0 },
    { 0xff2e, 192, TLS1_3_VERSION, 0 },
    { 0xff2f, 256, TLS1_3_VERSION, 0 },
    { 0xff30, 256, TLS1_3_VERSION, 0 },
    { 0xff31, 256, TLS1_3_VERSION, 0 },
    { 0xff0a, 128, TLS1_3_VERSION, 0 },
    { 0xff16, 128, TLS1_3_VERSION, 0 },
    { 0xff0b, 128, TLS1_3_VERSION, 0 },
    { 0xff17, 128, TLS1_3_VERSION, 0 },
    { 0xff0c, 192, TLS1_3_VERSION, 0 },
    { 0xff18, 192, TLS1_3_VERSION, 0 },
    { 0xff0d, 256, TLS1_3_VERSION, 0 },
    { 0xff19, 256, TLS1_3_VERSION, 0 },
    { 0xff0e, 128, TLS1_3_VERSION, 0 },
    { 0xff1a, 128, TLS1_3_VERSION, 0 },
    { 0xff0f, 128, TLS1_3_VERSION, 0 },
    { 0xff1b, 128, TLS1_3_VERSION, 0 },
    { 0xff10, 192, TLS1_3_VERSION, 0 },
    { 0xff1c, 192, TLS1_3_VERSION, 0 },
    { 0xff11, 256, TLS1_3_VERSION, 0 },
    { 0xff1d, 256, TLS1_3_VERSION, 0 },
    { 0xff12, 128, TLS1_3_VERSION, 0 },
    { 0xff1e, 128, TLS1_3_VERSION, 0 },
    { 0xff13, 128, TLS1_3_VERSION, 0 },
    { 0xff1f, 128, TLS1_3_VERSION, 0 },
    { 0xff14, 192, TLS1_3_VERSION, 0 },
    { 0xff20, 192, TLS1_3_VERSION, 0 },
    { 0xff15, 256, TLS1_3_VERSION, 0 },
    { 0xff21, 256, TLS1_3_VERSION, 0 },
    { 0xff3a, 128, TLS1_3_VERSION, 0 },
    { 0xff3b, 128, TLS1_3_VERSION, 0 },
    { 0xff3c, 128, TLS1_3_VERSION, 0 },
    { 0xff3d, 128, TLS1_3_VERSION, 0 },
    { 0xff3e, 128, TLS1_3_VERSION, 0 },
    { 0xff3f, 128, TLS1_3_VERSION, 0 },
    { 0xff40, 128, TLS1_3_VERSION, 0 },
    { 0xff41, 128, TLS1_3_VERSION, 0 },
    { 0xff42, 128, TLS1_3_VERSION, 0 },
    { 0xff43, 128, TLS1_3_VERSION, 0 },
    { 0xff44, 128, TLS1_3_VERSION, 0 },
    { 0xff45, 128, TLS1_3_VERSION, 0 },
    { 0xff46, 192, TLS1_3_VERSION, 0 },
    { 0xff47, 192, TLS1_3_VERSION, 0 },
    { 0xff48, 192, TLS1_3_VERSION, 0 },
    { 0xff49, 192, TLS1_3_VERSION, 0 },
    { 0xff4a, 192, TLS1_3_VERSION, 0 },
    { 0xff4b, 192, TLS1_3_VERSION, 0 },
    { 0xff4c, 192, TLS1_3_VERSION, 0 },
    { 0xff4d, 192, TLS1_3_VERSION, 0 },
    { 0xff4e, 256, TLS1_3_VERSION, 0 },
    { 0xff4f, 256, TLS1_3_VERSION, 0 },
    { 0xff51, 256, TLS1_3_VERSION, 0 },
    { 0xff52, 256, TLS1_3_VERSION, 0 },
///// OQS_TEMPLATE_FRAGMENT_SIGALG_ASSIGNMENTS_END
};

int oqs_patch_codepoints() {
    ///// OQS_TEMPLATE_FRAGMENT_CODEPOINT_PATCHING_START
   if (getenv("OQS_CODEPOINT_FRODO640AES")) oqs_group_list[0].group_id = atoi(getenv("OQS_CODEPOINT_FRODO640AES"));
   if (getenv("OQS_CODEPOINT_P256_FRODO640AES")) oqs_group_list[1].group_id = atoi(getenv("OQS_CODEPOINT_P256_FRODO640AES"));
   if (getenv("OQS_CODEPOINT_X25519_FRODO640AES")) oqs_group_list[2].group_id = atoi(getenv("OQS_CODEPOINT_X25519_FRODO640AES"));
   if (getenv("OQS_CODEPOINT_FRODO640SHAKE")) oqs_group_list[3].group_id = atoi(getenv("OQS_CODEPOINT_FRODO640SHAKE"));
   if (getenv("OQS_CODEPOINT_P256_FRODO640SHAKE")) oqs_group_list[4].group_id = atoi(getenv("OQS_CODEPOINT_P256_FRODO640SHAKE"));
   if (getenv("OQS_CODEPOINT_X25519_FRODO640SHAKE")) oqs_group_list[5].group_id = atoi(getenv("OQS_CODEPOINT_X25519_FRODO640SHAKE"));
   if (getenv("OQS_CODEPOINT_FRODO976AES")) oqs_group_list[6].group_id = atoi(getenv("OQS_CODEPOINT_FRODO976AES"));
   if (getenv("OQS_CODEPOINT_P384_FRODO976AES")) oqs_group_list[7].group_id = atoi(getenv("OQS_CODEPOINT_P384_FRODO976AES"));
   if (getenv("OQS_CODEPOINT_X448_FRODO976AES")) oqs_group_list[8].group_id = atoi(getenv("OQS_CODEPOINT_X448_FRODO976AES"));
   if (getenv("OQS_CODEPOINT_FRODO976SHAKE")) oqs_group_list[9].group_id = atoi(getenv("OQS_CODEPOINT_FRODO976SHAKE"));
   if (getenv("OQS_CODEPOINT_P384_FRODO976SHAKE")) oqs_group_list[10].group_id = atoi(getenv("OQS_CODEPOINT_P384_FRODO976SHAKE"));
   if (getenv("OQS_CODEPOINT_X448_FRODO976SHAKE")) oqs_group_list[11].group_id = atoi(getenv("OQS_CODEPOINT_X448_FRODO976SHAKE"));
   if (getenv("OQS_CODEPOINT_FRODO1344AES")) oqs_group_list[12].group_id = atoi(getenv("OQS_CODEPOINT_FRODO1344AES"));
   if (getenv("OQS_CODEPOINT_P521_FRODO1344AES")) oqs_group_list[13].group_id = atoi(getenv("OQS_CODEPOINT_P521_FRODO1344AES"));
   if (getenv("OQS_CODEPOINT_FRODO1344SHAKE")) oqs_group_list[14].group_id = atoi(getenv("OQS_CODEPOINT_FRODO1344SHAKE"));
   if (getenv("OQS_CODEPOINT_P521_FRODO1344SHAKE")) oqs_group_list[15].group_id = atoi(getenv("OQS_CODEPOINT_P521_FRODO1344SHAKE"));
   if (getenv("OQS_CODEPOINT_MLKEM512")) oqs_group_list[16].group_id = atoi(getenv("OQS_CODEPOINT_MLKEM512"));
   if (getenv("OQS_CODEPOINT_P256_MLKEM512")) oqs_group_list[17].group_id = atoi(getenv("OQS_CODEPOINT_P256_MLKEM512"));
   if (getenv("OQS_CODEPOINT_X25519_MLKEM512")) oqs_group_list[18].group_id = atoi(getenv("OQS_CODEPOINT_X25519_MLKEM512"));
   if (getenv("OQS_CODEPOINT_MLKEM768")) oqs_group_list[19].group_id = atoi(getenv("OQS_CODEPOINT_MLKEM768"));
   if (getenv("OQS_CODEPOINT_P384_MLKEM768")) oqs_group_list[20].group_id = atoi(getenv("OQS_CODEPOINT_P384_MLKEM768"));
   if (getenv("OQS_CODEPOINT_X448_MLKEM768")) oqs_group_list[21].group_id = atoi(getenv("OQS_CODEPOINT_X448_MLKEM768"));
   if (getenv("OQS_CODEPOINT_X25519MLKEM768")) oqs_group_list[22].group_id = atoi(getenv("OQS_CODEPOINT_X25519MLKEM768"));
   if (getenv("OQS_CODEPOINT_SECP256R1MLKEM768")) oqs_group_list[23].group_id = atoi(getenv("OQS_CODEPOINT_SECP256R1MLKEM768"));
   if (getenv("OQS_CODEPOINT_MLKEM1024")) oqs_group_list[24].group_id = atoi(getenv("OQS_CODEPOINT_MLKEM1024"));
   if (getenv("OQS_CODEPOINT_P521_MLKEM1024")) oqs_group_list[25].group_id = atoi(getenv("OQS_CODEPOINT_P521_MLKEM1024"));
   if (getenv("OQS_CODEPOINT_SECP384R1MLKEM1024")) oqs_group_list[26].group_id = atoi(getenv("OQS_CODEPOINT_SECP384R1MLKEM1024"));
   if (getenv("OQS_CODEPOINT_BIKEL1")) oqs_group_list[27].group_id = atoi(getenv("OQS_CODEPOINT_BIKEL1"));
   if (getenv("OQS_CODEPOINT_P256_BIKEL1")) oqs_group_list[28].group_id = atoi(getenv("OQS_CODEPOINT_P256_BIKEL1"));
   if (getenv("OQS_CODEPOINT_X25519_BIKEL1")) oqs_group_list[29].group_id = atoi(getenv("OQS_CODEPOINT_X25519_BIKEL1"));
   if (getenv("OQS_CODEPOINT_BIKEL3")) oqs_group_list[30].group_id = atoi(getenv("OQS_CODEPOINT_BIKEL3"));
   if (getenv("OQS_CODEPOINT_P384_BIKEL3")) oqs_group_list[31].group_id = atoi(getenv("OQS_CODEPOINT_P384_BIKEL3"));
   if (getenv("OQS_CODEPOINT_X448_BIKEL3")) oqs_group_list[32].group_id = atoi(getenv("OQS_CODEPOINT_X448_BIKEL3"));
   if (getenv("OQS_CODEPOINT_BIKEL5")) oqs_group_list[33].group_id = atoi(getenv("OQS_CODEPOINT_BIKEL5"));
   if (getenv("OQS_CODEPOINT_P521_BIKEL5")) oqs_group_list[34].group_id = atoi(getenv("OQS_CODEPOINT_P521_BIKEL5"));

   if (getenv("OQS_CODEPOINT_MLDSA44")) oqs_sigalg_list[0].code_point = atoi(getenv("OQS_CODEPOINT_MLDSA44"));
   if (getenv("OQS_CODEPOINT_P256_MLDSA44")) oqs_sigalg_list[1].code_point = atoi(getenv("OQS_CODEPOINT_P256_MLDSA44"));
   if (getenv("OQS_CODEPOINT_RSA3072_MLDSA44")) oqs_sigalg_list[2].code_point = atoi(getenv("OQS_CODEPOINT_RSA3072_MLDSA44"));
   if (getenv("OQS_CODEPOINT_MLDSA44_PSS2048")) oqs_sigalg_list[3].code_point = atoi(getenv("OQS_CODEPOINT_MLDSA44_PSS2048"));
   if (getenv("OQS_CODEPOINT_MLDSA44_RSA2048")) oqs_sigalg_list[4].code_point = atoi(getenv("OQS_CODEPOINT_MLDSA44_RSA2048"));
   if (getenv("OQS_CODEPOINT_MLDSA44_ED25519")) oqs_sigalg_list[5].code_point = atoi(getenv("OQS_CODEPOINT_MLDSA44_ED25519"));
   if (getenv("OQS_CODEPOINT_MLDSA44_P256")) oqs_sigalg_list[6].code_point = atoi(getenv("OQS_CODEPOINT_MLDSA44_P256"));
   if (getenv("OQS_CODEPOINT_MLDSA44_BP256")) oqs_sigalg_list[7].code_point = atoi(getenv("OQS_CODEPOINT_MLDSA44_BP256"));
   if (getenv("OQS_CODEPOINT_MLDSA65")) oqs_sigalg_list[8].code_point = atoi(getenv("OQS_CODEPOINT_MLDSA65"));
   if (getenv("OQS_CODEPOINT_P384_MLDSA65")) oqs_sigalg_list[9].code_point = atoi(getenv("OQS_CODEPOINT_P384_MLDSA65"));
   if (getenv("OQS_CODEPOINT_MLDSA65_PSS3072")) oqs_sigalg_list[10].code_point = atoi(getenv("OQS_CODEPOINT_MLDSA65_PSS3072"));
   if (getenv("OQS_CODEPOINT_MLDSA65_RSA3072")) oqs_sigalg_list[11].code_point = atoi(getenv("OQS_CODEPOINT_MLDSA65_RSA3072"));
   if (getenv("OQS_CODEPOINT_MLDSA65_P256")) oqs_sigalg_list[12].code_point = atoi(getenv("OQS_CODEPOINT_MLDSA65_P256"));
   if (getenv("OQS_CODEPOINT_MLDSA65_BP256")) oqs_sigalg_list[13].code_point = atoi(getenv("OQS_CODEPOINT_MLDSA65_BP256"));
   if (getenv("OQS_CODEPOINT_MLDSA65_ED25519")) oqs_sigalg_list[14].code_point = atoi(getenv("OQS_CODEPOINT_MLDSA65_ED25519"));
   if (getenv("OQS_CODEPOINT_MLDSA87")) oqs_sigalg_list[15].code_point = atoi(getenv("OQS_CODEPOINT_MLDSA87"));
   if (getenv("OQS_CODEPOINT_P521_MLDSA87")) oqs_sigalg_list[16].code_point = atoi(getenv("OQS_CODEPOINT_P521_MLDSA87"));
   if (getenv("OQS_CODEPOINT_MLDSA87_P384")) oqs_sigalg_list[17].code_point = atoi(getenv("OQS_CODEPOINT_MLDSA87_P384"));
   if (getenv("OQS_CODEPOINT_MLDSA87_BP384")) oqs_sigalg_list[18].code_point = atoi(getenv("OQS_CODEPOINT_MLDSA87_BP384"));
   if (getenv("OQS_CODEPOINT_MLDSA87_ED448")) oqs_sigalg_list[19].code_point = atoi(getenv("OQS_CODEPOINT_MLDSA87_ED448"));
   if (getenv("OQS_CODEPOINT_FALCON512")) oqs_sigalg_list[20].code_point = atoi(getenv("OQS_CODEPOINT_FALCON512"));
   if (getenv("OQS_CODEPOINT_P256_FALCON512")) oqs_sigalg_list[21].code_point = atoi(getenv("OQS_CODEPOINT_P256_FALCON512"));
   if (getenv("OQS_CODEPOINT_RSA3072_FALCON512")) oqs_sigalg_list[22].code_point = atoi(getenv("OQS_CODEPOINT_RSA3072_FALCON512"));
   if (getenv("OQS_CODEPOINT_FALCONPADDED512")) oqs_sigalg_list[23].code_point = atoi(getenv("OQS_CODEPOINT_FALCONPADDED512"));
   if (getenv("OQS_CODEPOINT_P256_FALCONPADDED512")) oqs_sigalg_list[24].code_point = atoi(getenv("OQS_CODEPOINT_P256_FALCONPADDED512"));
   if (getenv("OQS_CODEPOINT_RSA3072_FALCONPADDED512")) oqs_sigalg_list[25].code_point = atoi(getenv("OQS_CODEPOINT_RSA3072_FALCONPADDED512"));
   if (getenv("OQS_CODEPOINT_FALCON1024")) oqs_sigalg_list[26].code_point = atoi(getenv("OQS_CODEPOINT_FALCON1024"));
   if (getenv("OQS_CODEPOINT_P521_FALCON1024")) oqs_sigalg_list[27].code_point = atoi(getenv("OQS_CODEPOINT_P521_FALCON1024"));
   if (getenv("OQS_CODEPOINT_FALCONPADDED1024")) oqs_sigalg_list[28].code_point = atoi(getenv("OQS_CODEPOINT_FALCONPADDED1024"));
   if (getenv("OQS_CODEPOINT_P521_FALCONPADDED1024")) oqs_sigalg_list[29].code_point = atoi(getenv("OQS_CODEPOINT_P521_FALCONPADDED1024"));
   if (getenv("OQS_CODEPOINT_SPHINCSSHA2128FSIMPLE")) oqs_sigalg_list[30].code_point = atoi(getenv("OQS_CODEPOINT_SPHINCSSHA2128FSIMPLE"));
   if (getenv("OQS_CODEPOINT_P256_SPHINCSSHA2128FSIMPLE")) oqs_sigalg_list[31].code_point = atoi(getenv("OQS_CODEPOINT_P256_SPHINCSSHA2128FSIMPLE"));
   if (getenv("OQS_CODEPOINT_RSA3072_SPHINCSSHA2128FSIMPLE")) oqs_sigalg_list[32].code_point = atoi(getenv("OQS_CODEPOINT_RSA3072_SPHINCSSHA2128FSIMPLE"));
   if (getenv("OQS_CODEPOINT_SPHINCSSHA2128SSIMPLE")) oqs_sigalg_list[33].code_point = atoi(getenv("OQS_CODEPOINT_SPHINCSSHA2128SSIMPLE"));
   if (getenv("OQS_CODEPOINT_P256_SPHINCSSHA2128SSIMPLE")) oqs_sigalg_list[34].code_point = atoi(getenv("OQS_CODEPOINT_P256_SPHINCSSHA2128SSIMPLE"));
   if (getenv("OQS_CODEPOINT_RSA3072_SPHINCSSHA2128SSIMPLE")) oqs_sigalg_list[35].code_point = atoi(getenv("OQS_CODEPOINT_RSA3072_SPHINCSSHA2128SSIMPLE"));
   if (getenv("OQS_CODEPOINT_SPHINCSSHA2192FSIMPLE")) oqs_sigalg_list[36].code_point = atoi(getenv("OQS_CODEPOINT_SPHINCSSHA2192FSIMPLE"));
   if (getenv("OQS_CODEPOINT_P384_SPHINCSSHA2192FSIMPLE")) oqs_sigalg_list[37].code_point = atoi(getenv("OQS_CODEPOINT_P384_SPHINCSSHA2192FSIMPLE"));
   if (getenv("OQS_CODEPOINT_SPHINCSSHA2192SSIMPLE")) oqs_sigalg_list[38].code_point = atoi(getenv("OQS_CODEPOINT_SPHINCSSHA2192SSIMPLE"));
   if (getenv("OQS_CODEPOINT_P384_SPHINCSSHA2192SSIMPLE")) oqs_sigalg_list[39].code_point = atoi(getenv("OQS_CODEPOINT_P384_SPHINCSSHA2192SSIMPLE"));
   if (getenv("OQS_CODEPOINT_SPHINCSSHA2256FSIMPLE")) oqs_sigalg_list[40].code_point = atoi(getenv("OQS_CODEPOINT_SPHINCSSHA2256FSIMPLE"));
   if (getenv("OQS_CODEPOINT_P521_SPHINCSSHA2256FSIMPLE")) oqs_sigalg_list[41].code_point = atoi(getenv("OQS_CODEPOINT_P521_SPHINCSSHA2256FSIMPLE"));
   if (getenv("OQS_CODEPOINT_SPHINCSSHA2256SSIMPLE")) oqs_sigalg_list[42].code_point = atoi(getenv("OQS_CODEPOINT_SPHINCSSHA2256SSIMPLE"));
   if (getenv("OQS_CODEPOINT_P521_SPHINCSSHA2256SSIMPLE")) oqs_sigalg_list[43].code_point = atoi(getenv("OQS_CODEPOINT_P521_SPHINCSSHA2256SSIMPLE"));
   if (getenv("OQS_CODEPOINT_SPHINCSSHAKE128FSIMPLE")) oqs_sigalg_list[44].code_point = atoi(getenv("OQS_CODEPOINT_SPHINCSSHAKE128FSIMPLE"));
   if (getenv("OQS_CODEPOINT_P256_SPHINCSSHAKE128FSIMPLE")) oqs_sigalg_list[45].code_point = atoi(getenv("OQS_CODEPOINT_P256_SPHINCSSHAKE128FSIMPLE"));
   if (getenv("OQS_CODEPOINT_RSA3072_SPHINCSSHAKE128FSIMPLE")) oqs_sigalg_list[46].code_point = atoi(getenv("OQS_CODEPOINT_RSA3072_SPHINCSSHAKE128FSIMPLE"));
   if (getenv("OQS_CODEPOINT_SPHINCSSHAKE128SSIMPLE")) oqs_sigalg_list[47].code_point = atoi(getenv("OQS_CODEPOINT_SPHINCSSHAKE128SSIMPLE"));
   if (getenv("OQS_CODEPOINT_P256_SPHINCSSHAKE128SSIMPLE")) oqs_sigalg_list[48].code_point = atoi(getenv("OQS_CODEPOINT_P256_SPHINCSSHAKE128SSIMPLE"));
   if (getenv("OQS_CODEPOINT_RSA3072_SPHINCSSHAKE128SSIMPLE")) oqs_sigalg_list[49].code_point = atoi(getenv("OQS_CODEPOINT_RSA3072_SPHINCSSHAKE128SSIMPLE"));
   if (getenv("OQS_CODEPOINT_SPHINCSSHAKE192FSIMPLE")) oqs_sigalg_list[50].code_point = atoi(getenv("OQS_CODEPOINT_SPHINCSSHAKE192FSIMPLE"));
   if (getenv("OQS_CODEPOINT_P384_SPHINCSSHAKE192FSIMPLE")) oqs_sigalg_list[51].code_point = atoi(getenv("OQS_CODEPOINT_P384_SPHINCSSHAKE192FSIMPLE"));
   if (getenv("OQS_CODEPOINT_SPHINCSSHAKE192SSIMPLE")) oqs_sigalg_list[52].code_point = atoi(getenv("OQS_CODEPOINT_SPHINCSSHAKE192SSIMPLE"));
   if (getenv("OQS_CODEPOINT_P384_SPHINCSSHAKE192SSIMPLE")) oqs_sigalg_list[53].code_point = atoi(getenv("OQS_CODEPOINT_P384_SPHINCSSHAKE192SSIMPLE"));
   if (getenv("OQS_CODEPOINT_SPHINCSSHAKE256FSIMPLE")) oqs_sigalg_list[54].code_point = atoi(getenv("OQS_CODEPOINT_SPHINCSSHAKE256FSIMPLE"));
   if (getenv("OQS_CODEPOINT_P521_SPHINCSSHAKE256FSIMPLE")) oqs_sigalg_list[55].code_point = atoi(getenv("OQS_CODEPOINT_P521_SPHINCSSHAKE256FSIMPLE"));
   if (getenv("OQS_CODEPOINT_SPHINCSSHAKE256SSIMPLE")) oqs_sigalg_list[56].code_point = atoi(getenv("OQS_CODEPOINT_SPHINCSSHAKE256SSIMPLE"));
   if (getenv("OQS_CODEPOINT_P521_SPHINCSSHAKE256SSIMPLE")) oqs_sigalg_list[57].code_point = atoi(getenv("OQS_CODEPOINT_P521_SPHINCSSHAKE256SSIMPLE"));
   if (getenv("OQS_CODEPOINT_MAYO1")) oqs_sigalg_list[58].code_point = atoi(getenv("OQS_CODEPOINT_MAYO1"));
   if (getenv("OQS_CODEPOINT_P256_MAYO1")) oqs_sigalg_list[59].code_point = atoi(getenv("OQS_CODEPOINT_P256_MAYO1"));
   if (getenv("OQS_CODEPOINT_MAYO2")) oqs_sigalg_list[60].code_point = atoi(getenv("OQS_CODEPOINT_MAYO2"));
   if (getenv("OQS_CODEPOINT_P256_MAYO2")) oqs_sigalg_list[61].code_point = atoi(getenv("OQS_CODEPOINT_P256_MAYO2"));
   if (getenv("OQS_CODEPOINT_MAYO3")) oqs_sigalg_list[62].code_point = atoi(getenv("OQS_CODEPOINT_MAYO3"));
   if (getenv("OQS_CODEPOINT_P384_MAYO3")) oqs_sigalg_list[63].code_point = atoi(getenv("OQS_CODEPOINT_P384_MAYO3"));
   if (getenv("OQS_CODEPOINT_MAYO5")) oqs_sigalg_list[64].code_point = atoi(getenv("OQS_CODEPOINT_MAYO5"));
   if (getenv("OQS_CODEPOINT_P521_MAYO5")) oqs_sigalg_list[65].code_point = atoi(getenv("OQS_CODEPOINT_P521_MAYO5"));
   if (getenv("OQS_CODEPOINT_CROSSRSDP128BALANCED")) oqs_sigalg_list[66].code_point = atoi(getenv("OQS_CODEPOINT_CROSSRSDP128BALANCED"));
   if (getenv("OQS_CODEPOINT_CROSSRSDP128FAST")) oqs_sigalg_list[67].code_point = atoi(getenv("OQS_CODEPOINT_CROSSRSDP128FAST"));
   if (getenv("OQS_CODEPOINT_CROSSRSDP128SMALL")) oqs_sigalg_list[68].code_point = atoi(getenv("OQS_CODEPOINT_CROSSRSDP128SMALL"));
   if (getenv("OQS_CODEPOINT_CROSSRSDP192BALANCED")) oqs_sigalg_list[69].code_point = atoi(getenv("OQS_CODEPOINT_CROSSRSDP192BALANCED"));
   if (getenv("OQS_CODEPOINT_CROSSRSDP192FAST")) oqs_sigalg_list[70].code_point = atoi(getenv("OQS_CODEPOINT_CROSSRSDP192FAST"));
   if (getenv("OQS_CODEPOINT_CROSSRSDP192SMALL")) oqs_sigalg_list[71].code_point = atoi(getenv("OQS_CODEPOINT_CROSSRSDP192SMALL"));
   if (getenv("OQS_CODEPOINT_CROSSRSDP256BALANCED")) oqs_sigalg_list[72].code_point = atoi(getenv("OQS_CODEPOINT_CROSSRSDP256BALANCED"));
   if (getenv("OQS_CODEPOINT_CROSSRSDP256FAST")) oqs_sigalg_list[73].code_point = atoi(getenv("OQS_CODEPOINT_CROSSRSDP256FAST"));
   if (getenv("OQS_CODEPOINT_CROSSRSDP256SMALL")) oqs_sigalg_list[74].code_point = atoi(getenv("OQS_CODEPOINT_CROSSRSDP256SMALL"));
   if (getenv("OQS_CODEPOINT_CROSSRSDPG128BALANCED")) oqs_sigalg_list[75].code_point = atoi(getenv("OQS_CODEPOINT_CROSSRSDPG128BALANCED"));
   if (getenv("OQS_CODEPOINT_CROSSRSDPG128FAST")) oqs_sigalg_list[76].code_point = atoi(getenv("OQS_CODEPOINT_CROSSRSDPG128FAST"));
   if (getenv("OQS_CODEPOINT_CROSSRSDPG128SMALL")) oqs_sigalg_list[77].code_point = atoi(getenv("OQS_CODEPOINT_CROSSRSDPG128SMALL"));
   if (getenv("OQS_CODEPOINT_CROSSRSDPG192BALANCED")) oqs_sigalg_list[78].code_point = atoi(getenv("OQS_CODEPOINT_CROSSRSDPG192BALANCED"));
   if (getenv("OQS_CODEPOINT_CROSSRSDPG192FAST")) oqs_sigalg_list[79].code_point = atoi(getenv("OQS_CODEPOINT_CROSSRSDPG192FAST"));
   if (getenv("OQS_CODEPOINT_CROSSRSDPG192SMALL")) oqs_sigalg_list[80].code_point = atoi(getenv("OQS_CODEPOINT_CROSSRSDPG192SMALL"));
   if (getenv("OQS_CODEPOINT_CROSSRSDPG256BALANCED")) oqs_sigalg_list[81].code_point = atoi(getenv("OQS_CODEPOINT_CROSSRSDPG256BALANCED"));
   if (getenv("OQS_CODEPOINT_CROSSRSDPG256FAST")) oqs_sigalg_list[82].code_point = atoi(getenv("OQS_CODEPOINT_CROSSRSDPG256FAST"));
   if (getenv("OQS_CODEPOINT_CROSSRSDPG256SMALL")) oqs_sigalg_list[83].code_point = atoi(getenv("OQS_CODEPOINT_CROSSRSDPG256SMALL"));
   if (getenv("OQS_CODEPOINT_OV_IS")) oqs_sigalg_list[84].code_point = atoi(getenv("OQS_CODEPOINT_OV_IS"));
   if (getenv("OQS_CODEPOINT_P256_OV_IS")) oqs_sigalg_list[85].code_point = atoi(getenv("OQS_CODEPOINT_P256_OV_IS"));
   if (getenv("OQS_CODEPOINT_OV_IP")) oqs_sigalg_list[86].code_point = atoi(getenv("OQS_CODEPOINT_OV_IP"));
   if (getenv("OQS_CODEPOINT_P256_OV_IP")) oqs_sigalg_list[87].code_point = atoi(getenv("OQS_CODEPOINT_P256_OV_IP"));
   if (getenv("OQS_CODEPOINT_OV_III")) oqs_sigalg_list[88].code_point = atoi(getenv("OQS_CODEPOINT_OV_III"));
   if (getenv("OQS_CODEPOINT_P384_OV_III")) oqs_sigalg_list[89].code_point = atoi(getenv("OQS_CODEPOINT_P384_OV_III"));
   if (getenv("OQS_CODEPOINT_OV_V")) oqs_sigalg_list[90].code_point = atoi(getenv("OQS_CODEPOINT_OV_V"));
   if (getenv("OQS_CODEPOINT_P521_OV_V")) oqs_sigalg_list[91].code_point = atoi(getenv("OQS_CODEPOINT_P521_OV_V"));
   if (getenv("OQS_CODEPOINT_OV_IS_PKC")) oqs_sigalg_list[92].code_point = atoi(getenv("OQS_CODEPOINT_OV_IS_PKC"));
   if (getenv("OQS_CODEPOINT_P256_OV_IS_PKC")) oqs_sigalg_list[93].code_point = atoi(getenv("OQS_CODEPOINT_P256_OV_IS_PKC"));
   if (getenv("OQS_CODEPOINT_OV_IP_PKC")) oqs_sigalg_list[94].code_point = atoi(getenv("OQS_CODEPOINT_OV_IP_PKC"));
   if (getenv("OQS_CODEPOINT_P256_OV_IP_PKC")) oqs_sigalg_list[95].code_point = atoi(getenv("OQS_CODEPOINT_P256_OV_IP_PKC"));
   if (getenv("OQS_CODEPOINT_OV_III_PKC")) oqs_sigalg_list[96].code_point = atoi(getenv("OQS_CODEPOINT_OV_III_PKC"));
   if (getenv("OQS_CODEPOINT_P384_OV_III_PKC")) oqs_sigalg_list[97].code_point = atoi(getenv("OQS_CODEPOINT_P384_OV_III_PKC"));
   if (getenv("OQS_CODEPOINT_OV_V_PKC")) oqs_sigalg_list[98].code_point = atoi(getenv("OQS_CODEPOINT_OV_V_PKC"));
   if (getenv("OQS_CODEPOINT_P521_OV_V_PKC")) oqs_sigalg_list[99].code_point = atoi(getenv("OQS_CODEPOINT_P521_OV_V_PKC"));
   if (getenv("OQS_CODEPOINT_OV_IS_PKC_SKC")) oqs_sigalg_list[100].code_point = atoi(getenv("OQS_CODEPOINT_OV_IS_PKC_SKC"));
   if (getenv("OQS_CODEPOINT_P256_OV_IS_PKC_SKC")) oqs_sigalg_list[101].code_point = atoi(getenv("OQS_CODEPOINT_P256_OV_IS_PKC_SKC"));
   if (getenv("OQS_CODEPOINT_OV_IP_PKC_SKC")) oqs_sigalg_list[102].code_point = atoi(getenv("OQS_CODEPOINT_OV_IP_PKC_SKC"));
   if (getenv("OQS_CODEPOINT_P256_OV_IP_PKC_SKC")) oqs_sigalg_list[103].code_point = atoi(getenv("OQS_CODEPOINT_P256_OV_IP_PKC_SKC"));
   if (getenv("OQS_CODEPOINT_OV_III_PKC_SKC")) oqs_sigalg_list[104].code_point = atoi(getenv("OQS_CODEPOINT_OV_III_PKC_SKC"));
   if (getenv("OQS_CODEPOINT_P384_OV_III_PKC_SKC")) oqs_sigalg_list[105].code_point = atoi(getenv("OQS_CODEPOINT_P384_OV_III_PKC_SKC"));
   if (getenv("OQS_CODEPOINT_OV_V_PKC_SKC")) oqs_sigalg_list[106].code_point = atoi(getenv("OQS_CODEPOINT_OV_V_PKC_SKC"));
   if (getenv("OQS_CODEPOINT_P521_OV_V_PKC_SKC")) oqs_sigalg_list[107].code_point = atoi(getenv("OQS_CODEPOINT_P521_OV_V_PKC_SKC"));
   if (getenv("OQS_CODEPOINT_SNOVA2454")) oqs_sigalg_list[108].code_point = atoi(getenv("OQS_CODEPOINT_SNOVA2454"));
   if (getenv("OQS_CODEPOINT_P256_SNOVA2454")) oqs_sigalg_list[109].code_point = atoi(getenv("OQS_CODEPOINT_P256_SNOVA2454"));
   if (getenv("OQS_CODEPOINT_SNOVA2454SHAKE")) oqs_sigalg_list[110].code_point = atoi(getenv("OQS_CODEPOINT_SNOVA2454SHAKE"));
   if (getenv("OQS_CODEPOINT_P256_SNOVA2454SHAKE")) oqs_sigalg_list[111].code_point = atoi(getenv("OQS_CODEPOINT_P256_SNOVA2454SHAKE"));
   if (getenv("OQS_CODEPOINT_SNOVA2454ESK")) oqs_sigalg_list[112].code_point = atoi(getenv("OQS_CODEPOINT_SNOVA2454ESK"));
   if (getenv("OQS_CODEPOINT_P256_SNOVA2454ESK")) oqs_sigalg_list[113].code_point = atoi(getenv("OQS_CODEPOINT_P256_SNOVA2454ESK"));
   if (getenv("OQS_CODEPOINT_SNOVA2454SHAKEESK")) oqs_sigalg_list[114].code_point = atoi(getenv("OQS_CODEPOINT_SNOVA2454SHAKEESK"));
   if (getenv("OQS_CODEPOINT_P256_SNOVA2454SHAKEESK")) oqs_sigalg_list[115].code_point = atoi(getenv("OQS_CODEPOINT_P256_SNOVA2454SHAKEESK"));
   if (getenv("OQS_CODEPOINT_SNOVA37172")) oqs_sigalg_list[116].code_point = atoi(getenv("OQS_CODEPOINT_SNOVA37172"));
   if (getenv("OQS_CODEPOINT_P256_SNOVA37172")) oqs_sigalg_list[117].code_point = atoi(getenv("OQS_CODEPOINT_P256_SNOVA37172"));
   if (getenv("OQS_CODEPOINT_SNOVA2583")) oqs_sigalg_list[118].code_point = atoi(getenv("OQS_CODEPOINT_SNOVA2583"));
   if (getenv("OQS_CODEPOINT_P256_SNOVA2583")) oqs_sigalg_list[119].code_point = atoi(getenv("OQS_CODEPOINT_P256_SNOVA2583"));
   if (getenv("OQS_CODEPOINT_SNOVA56252")) oqs_sigalg_list[120].code_point = atoi(getenv("OQS_CODEPOINT_SNOVA56252"));
   if (getenv("OQS_CODEPOINT_P384_SNOVA56252")) oqs_sigalg_list[121].code_point = atoi(getenv("OQS_CODEPOINT_P384_SNOVA56252"));
   if (getenv("OQS_CODEPOINT_SNOVA49113")) oqs_sigalg_list[122].code_point = atoi(getenv("OQS_CODEPOINT_SNOVA49113"));
   if (getenv("OQS_CODEPOINT_P384_SNOVA49113")) oqs_sigalg_list[123].code_point = atoi(getenv("OQS_CODEPOINT_P384_SNOVA49113"));
   if (getenv("OQS_CODEPOINT_SNOVA3784")) oqs_sigalg_list[124].code_point = atoi(getenv("OQS_CODEPOINT_SNOVA3784"));
   if (getenv("OQS_CODEPOINT_P384_SNOVA3784")) oqs_sigalg_list[125].code_point = atoi(getenv("OQS_CODEPOINT_P384_SNOVA3784"));
   if (getenv("OQS_CODEPOINT_SNOVA2455")) oqs_sigalg_list[126].code_point = atoi(getenv("OQS_CODEPOINT_SNOVA2455"));
   if (getenv("OQS_CODEPOINT_P384_SNOVA2455")) oqs_sigalg_list[127].code_point = atoi(getenv("OQS_CODEPOINT_P384_SNOVA2455"));
   if (getenv("OQS_CODEPOINT_SNOVA60104")) oqs_sigalg_list[128].code_point = atoi(getenv("OQS_CODEPOINT_SNOVA60104"));
   if (getenv("OQS_CODEPOINT_P521_SNOVA60104")) oqs_sigalg_list[129].code_point = atoi(getenv("OQS_CODEPOINT_P521_SNOVA60104"));
   if (getenv("OQS_CODEPOINT_SNOVA2965")) oqs_sigalg_list[130].code_point = atoi(getenv("OQS_CODEPOINT_SNOVA2965"));
   if (getenv("OQS_CODEPOINT_P521_SNOVA2965")) oqs_sigalg_list[131].code_point = atoi(getenv("OQS_CODEPOINT_P521_SNOVA2965"));
///// OQS_TEMPLATE_FRAGMENT_CODEPOINT_PATCHING_END
    return 1;
}

static int oqs_group_capability(OSSL_CALLBACK *cb, void *arg) {
    size_t i;

    for (i = 0; i < OSSL_NELEM(oqs_param_group_list); i++) {
        // do not register algorithms disabled at runtime
        if (sk_OPENSSL_STRING_find(oqsprov_get_rt_disabled_algs(),
                                   (char *)oqs_param_group_list[i][2].data) <
            0) {
            if (!cb(oqs_param_group_list[i], arg))
                return 0;
        }
    }

    return 1;
}

#ifdef OSSL_CAPABILITY_TLS_SIGALG_NAME
#define OQS_SIGALG_ENTRY(tlsname, realname, algorithm, oid, idx)               \
    {                                                                          \
        OSSL_PARAM_utf8_string(OSSL_CAPABILITY_TLS_SIGALG_IANA_NAME, #tlsname, \
                               sizeof(#tlsname)),                              \
            OSSL_PARAM_utf8_string(OSSL_CAPABILITY_TLS_SIGALG_NAME, #tlsname,  \
                                   sizeof(#tlsname)),                          \
            OSSL_PARAM_utf8_string(OSSL_CAPABILITY_TLS_SIGALG_OID, #oid,       \
                                   sizeof(#oid)),                              \
            OSSL_PARAM_uint(OSSL_CAPABILITY_TLS_SIGALG_CODE_POINT,             \
                            (unsigned int *)&oqs_sigalg_list[idx].code_point), \
            OSSL_PARAM_uint(OSSL_CAPABILITY_TLS_SIGALG_SECURITY_BITS,          \
                            (unsigned int *)&oqs_sigalg_list[idx].secbits),    \
            OSSL_PARAM_int(OSSL_CAPABILITY_TLS_SIGALG_MIN_TLS,                 \
                           (unsigned int *)&oqs_sigalg_list[idx].mintls),      \
            OSSL_PARAM_int(OSSL_CAPABILITY_TLS_SIGALG_MAX_TLS,                 \
                           (unsigned int *)&oqs_sigalg_list[idx].maxtls),      \
            OSSL_PARAM_END                                                     \
    }

static const OSSL_PARAM oqs_param_sigalg_list[][12] = {
///// OQS_TEMPLATE_FRAGMENT_SIGALG_NAMES_START
#ifdef OQS_ENABLE_SIG_ml_dsa_44
    OQS_SIGALG_ENTRY(mldsa44, mldsa44, mldsa44, "2.16.840.1.101.3.4.3.17", 0),
    OQS_SIGALG_ENTRY(p256_mldsa44, p256_mldsa44, p256_mldsa44, "1.3.9999.7.5", 1),
    OQS_SIGALG_ENTRY(rsa3072_mldsa44, rsa3072_mldsa44, rsa3072_mldsa44, "1.3.9999.7.6", 2),
    OQS_SIGALG_ENTRY(mldsa44_pss2048, mldsa44_pss2048, mldsa44_pss2048, "2.16.840.1.114027.80.8.1.1", 3),
    OQS_SIGALG_ENTRY(mldsa44_rsa2048, mldsa44_rsa2048, mldsa44_rsa2048, "2.16.840.1.114027.80.8.1.2", 4),
    OQS_SIGALG_ENTRY(mldsa44_ed25519, mldsa44_ed25519, mldsa44_ed25519, "2.16.840.1.114027.80.8.1.3", 5),
    OQS_SIGALG_ENTRY(mldsa44_p256, mldsa44_p256, mldsa44_p256, "2.16.840.1.114027.80.8.1.4", 6),
    OQS_SIGALG_ENTRY(mldsa44_bp256, mldsa44_bp256, mldsa44_bp256, "2.16.840.1.114027.80.8.1.5", 7),
#endif
#ifdef OQS_ENABLE_SIG_ml_dsa_65
    OQS_SIGALG_ENTRY(mldsa65, mldsa65, mldsa65, "2.16.840.1.101.3.4.3.18", 8),
    OQS_SIGALG_ENTRY(p384_mldsa65, p384_mldsa65, p384_mldsa65, "1.3.9999.7.7", 9),
    OQS_SIGALG_ENTRY(mldsa65_pss3072, mldsa65_pss3072, mldsa65_pss3072, "2.16.840.1.114027.80.8.1.6", 10),
    OQS_SIGALG_ENTRY(mldsa65_rsa3072, mldsa65_rsa3072, mldsa65_rsa3072, "2.16.840.1.114027.80.8.1.7", 11),
    OQS_SIGALG_ENTRY(mldsa65_p256, mldsa65_p256, mldsa65_p256, "2.16.840.1.114027.80.8.1.8", 12),
    OQS_SIGALG_ENTRY(mldsa65_bp256, mldsa65_bp256, mldsa65_bp256, "2.16.840.1.114027.80.8.1.9", 13),
    OQS_SIGALG_ENTRY(mldsa65_ed25519, mldsa65_ed25519, mldsa65_ed25519, "2.16.840.1.114027.80.8.1.10", 14),
#endif
#ifdef OQS_ENABLE_SIG_ml_dsa_87
    OQS_SIGALG_ENTRY(mldsa87, mldsa87, mldsa87, "2.16.840.1.101.3.4.3.19", 15),
    OQS_SIGALG_ENTRY(p521_mldsa87, p521_mldsa87, p521_mldsa87, "1.3.9999.7.8", 16),
    OQS_SIGALG_ENTRY(mldsa87_p384, mldsa87_p384, mldsa87_p384, "2.16.840.1.114027.80.8.1.11", 17),
    OQS_SIGALG_ENTRY(mldsa87_bp384, mldsa87_bp384, mldsa87_bp384, "2.16.840.1.114027.80.8.1.12", 18),
    OQS_SIGALG_ENTRY(mldsa87_ed448, mldsa87_ed448, mldsa87_ed448, "2.16.840.1.114027.80.8.1.13", 19),
#endif
#ifdef OQS_ENABLE_SIG_falcon_512
    OQS_SIGALG_ENTRY(falcon512, falcon512, falcon512, "1.3.9999.3.11", 20),
    OQS_SIGALG_ENTRY(p256_falcon512, p256_falcon512, p256_falcon512, "1.3.9999.3.12", 21),
    OQS_SIGALG_ENTRY(rsa3072_falcon512, rsa3072_falcon512, rsa3072_falcon512, "1.3.9999.3.13", 22),
#endif
#ifdef OQS_ENABLE_SIG_falcon_padded_512
    OQS_SIGALG_ENTRY(falconpadded512, falconpadded512, falconpadded512, "1.3.9999.3.16", 23),
    OQS_SIGALG_ENTRY(p256_falconpadded512, p256_falconpadded512, p256_falconpadded512, "1.3.9999.3.17", 24),
    OQS_SIGALG_ENTRY(rsa3072_falconpadded512, rsa3072_falconpadded512, rsa3072_falconpadded512, "1.3.9999.3.18", 25),
#endif
#ifdef OQS_ENABLE_SIG_falcon_1024
    OQS_SIGALG_ENTRY(falcon1024, falcon1024, falcon1024, "1.3.9999.3.14", 26),
    OQS_SIGALG_ENTRY(p521_falcon1024, p521_falcon1024, p521_falcon1024, "1.3.9999.3.15", 27),
#endif
#ifdef OQS_ENABLE_SIG_falcon_padded_1024
    OQS_SIGALG_ENTRY(falconpadded1024, falconpadded1024, falconpadded1024, "1.3.9999.3.19", 28),
    OQS_SIGALG_ENTRY(p521_falconpadded1024, p521_falconpadded1024, p521_falconpadded1024, "1.3.9999.3.20", 29),
#endif
#ifdef OQS_ENABLE_SIG_sphincs_sha2_128f_simple
    OQS_SIGALG_ENTRY(sphincssha2128fsimple, sphincssha2128fsimple, sphincssha2128fsimple, "1.3.9999.6.4.13", 30),
    OQS_SIGALG_ENTRY(p256_sphincssha2128fsimple, p256_sphincssha2128fsimple, p256_sphincssha2128fsimple, "1.3.9999.6.4.14", 31),
    OQS_SIGALG_ENTRY(rsa3072_sphincssha2128fsimple, rsa3072_sphincssha2128fsimple, rsa3072_sphincssha2128fsimple, "1.3.9999.6.4.15", 32),
#endif
#ifdef OQS_ENABLE_SIG_sphincs_sha2_128s_simple
    OQS_SIGALG_ENTRY(sphincssha2128ssimple, sphincssha2128ssimple, sphincssha2128ssimple, "1.3.9999.6.4.16", 33),
    OQS_SIGALG_ENTRY(p256_sphincssha2128ssimple, p256_sphincssha2128ssimple, p256_sphincssha2128ssimple, "1.3.9999.6.4.17", 34),
    OQS_SIGALG_ENTRY(rsa3072_sphincssha2128ssimple, rsa3072_sphincssha2128ssimple, rsa3072_sphincssha2128ssimple, "1.3.9999.6.4.18", 35),
#endif
#ifdef OQS_ENABLE_SIG_sphincs_sha2_192f_simple
    OQS_SIGALG_ENTRY(sphincssha2192fsimple, sphincssha2192fsimple, sphincssha2192fsimple, "1.3.9999.6.5.10", 36),
    OQS_SIGALG_ENTRY(p384_sphincssha2192fsimple, p384_sphincssha2192fsimple, p384_sphincssha2192fsimple, "1.3.9999.6.5.11", 37),
#endif
#ifdef OQS_ENABLE_SIG_sphincs_sha2_192s_simple
    OQS_SIGALG_ENTRY(sphincssha2192ssimple, sphincssha2192ssimple, sphincssha2192ssimple, "1.3.9999.6.5.12", 38),
    OQS_SIGALG_ENTRY(p384_sphincssha2192ssimple, p384_sphincssha2192ssimple, p384_sphincssha2192ssimple, "1.3.9999.6.5.13", 39),
#endif
#ifdef OQS_ENABLE_SIG_sphincs_sha2_256f_simple
    OQS_SIGALG_ENTRY(sphincssha2256fsimple, sphincssha2256fsimple, sphincssha2256fsimple, "1.3.9999.6.6.10", 40),
    OQS_SIGALG_ENTRY(p521_sphincssha2256fsimple, p521_sphincssha2256fsimple, p521_sphincssha2256fsimple, "1.3.9999.6.6.11", 41),
#endif
#ifdef OQS_ENABLE_SIG_sphincs_sha2_256s_simple
    OQS_SIGALG_ENTRY(sphincssha2256ssimple, sphincssha2256ssimple, sphincssha2256ssimple, "1.3.9999.6.6.12", 42),
    OQS_SIGALG_ENTRY(p521_sphincssha2256ssimple, p521_sphincssha2256ssimple, p521_sphincssha2256ssimple, "1.3.9999.6.6.13", 43),
#endif
#ifdef OQS_ENABLE_SIG_sphincs_shake_128f_simple
    OQS_SIGALG_ENTRY(sphincsshake128fsimple, sphincsshake128fsimple, sphincsshake128fsimple, "1.3.9999.6.7.13", 44),
    OQS_SIGALG_ENTRY(p256_sphincsshake128fsimple, p256_sphincsshake128fsimple, p256_sphincsshake128fsimple, "1.3.9999.6.7.14", 45),
    OQS_SIGALG_ENTRY(rsa3072_sphincsshake128fsimple, rsa3072_sphincsshake128fsimple, rsa3072_sphincsshake128fsimple, "1.3.9999.6.7.15", 46),
#endif
#ifdef OQS_ENABLE_SIG_sphincs_shake_128s_simple
    OQS_SIGALG_ENTRY(sphincsshake128ssimple, sphincsshake128ssimple, sphincsshake128ssimple, "1.3.9999.6.7.16", 47),
    OQS_SIGALG_ENTRY(p256_sphincsshake128ssimple, p256_sphincsshake128ssimple, p256_sphincsshake128ssimple, "1.3.9999.6.7.17", 48),
    OQS_SIGALG_ENTRY(rsa3072_sphincsshake128ssimple, rsa3072_sphincsshake128ssimple, rsa3072_sphincsshake128ssimple, "1.3.9999.6.7.18", 49),
#endif
#ifdef OQS_ENABLE_SIG_sphincs_shake_192f_simple
    OQS_SIGALG_ENTRY(sphincsshake192fsimple, sphincsshake192fsimple, sphincsshake192fsimple, "1.3.9999.6.8.10", 50),
    OQS_SIGALG_ENTRY(p384_sphincsshake192fsimple, p384_sphincsshake192fsimple, p384_sphincsshake192fsimple, "1.3.9999.6.8.11", 51),
#endif
#ifdef OQS_ENABLE_SIG_sphincs_shake_192s_simple
    OQS_SIGALG_ENTRY(sphincsshake192ssimple, sphincsshake192ssimple, sphincsshake192ssimple, "1.3.9999.6.8.12", 52),
    OQS_SIGALG_ENTRY(p384_sphincsshake192ssimple, p384_sphincsshake192ssimple, p384_sphincsshake192ssimple, "1.3.9999.6.8.13", 53),
#endif
#ifdef OQS_ENABLE_SIG_sphincs_shake_256f_simple
    OQS_SIGALG_ENTRY(sphincsshake256fsimple, sphincsshake256fsimple, sphincsshake256fsimple, "1.3.9999.6.9.10", 54),
    OQS_SIGALG_ENTRY(p521_sphincsshake256fsimple, p521_sphincsshake256fsimple, p521_sphincsshake256fsimple, "1.3.9999.6.9.11", 55),
#endif
#ifdef OQS_ENABLE_SIG_sphincs_shake_256s_simple
    OQS_SIGALG_ENTRY(sphincsshake256ssimple, sphincsshake256ssimple, sphincsshake256ssimple, "1.3.9999.6.9.12", 56),
    OQS_SIGALG_ENTRY(p521_sphincsshake256ssimple, p521_sphincsshake256ssimple, p521_sphincsshake256ssimple, "1.3.9999.6.9.13", 57),
#endif
#ifdef OQS_ENABLE_SIG_mayo_1
    OQS_SIGALG_ENTRY(mayo1, mayo1, mayo1, "1.3.9999.8.1.3", 58),
    OQS_SIGALG_ENTRY(p256_mayo1, p256_mayo1, p256_mayo1, "1.3.9999.8.1.4", 59),
#endif
#ifdef OQS_ENABLE_SIG_mayo_2
    OQS_SIGALG_ENTRY(mayo2, mayo2, mayo2, "1.3.9999.8.2.3", 60),
    OQS_SIGALG_ENTRY(p256_mayo2, p256_mayo2, p256_mayo2, "1.3.9999.8.2.4", 61),
#endif
#ifdef OQS_ENABLE_SIG_mayo_3
    OQS_SIGALG_ENTRY(mayo3, mayo3, mayo3, "1.3.9999.8.3.3", 62),
    OQS_SIGALG_ENTRY(p384_mayo3, p384_mayo3, p384_mayo3, "1.3.9999.8.3.4", 63),
#endif
#ifdef OQS_ENABLE_SIG_mayo_5
    OQS_SIGALG_ENTRY(mayo5, mayo5, mayo5, "1.3.9999.8.5.3", 64),
    OQS_SIGALG_ENTRY(p521_mayo5, p521_mayo5, p521_mayo5, "1.3.9999.8.5.4", 65),
#endif
#ifdef OQS_ENABLE_SIG_cross_rsdp_128_balanced
    OQS_SIGALG_ENTRY(CROSSrsdp128balanced, CROSSrsdp128balanced, CROSSrsdp128balanced, "1.3.6.1.4.1.62245.2.1.1.2", 66),
#endif
#ifdef OQS_ENABLE_SIG_cross_rsdp_128_fast
    OQS_SIGALG_ENTRY(CROSSrsdp128fast, CROSSrsdp128fast, CROSSrsdp128fast, "1.3.6.1.4.1.62245.2.1.2.2", 67),
#endif
#ifdef OQS_ENABLE_SIG_cross_rsdp_128_small
    OQS_SIGALG_ENTRY(CROSSrsdp128small, CROSSrsdp128small, CROSSrsdp128small, "1.3.6.1.4.1.62245.2.1.3.2", 68),
#endif
#ifdef OQS_ENABLE_SIG_cross_rsdp_192_balanced
    OQS_SIGALG_ENTRY(CROSSrsdp192balanced, CROSSrsdp192balanced, CROSSrsdp192balanced, "1.3.6.1.4.1.62245.2.1.4.2", 69),
#endif
#ifdef OQS_ENABLE_SIG_cross_rsdp_192_fast
    OQS_SIGALG_ENTRY(CROSSrsdp192fast, CROSSrsdp192fast, CROSSrsdp192fast, "1.3.6.1.4.1.62245.2.1.5.2", 70),
#endif
#ifdef OQS_ENABLE_SIG_cross_rsdp_192_small
    OQS_SIGALG_ENTRY(CROSSrsdp192small, CROSSrsdp192small, CROSSrsdp192small, "1.3.6.1.4.1.62245.2.1.6.2", 71),
#endif
#ifdef OQS_ENABLE_SIG_cross_rsdpg_128_balanced
    OQS_SIGALG_ENTRY(CROSSrsdpg128balanced, CROSSrsdpg128balanced, CROSSrsdpg128balanced, "1.3.6.1.4.1.62245.2.1.10.2", 72),
#endif
#ifdef OQS_ENABLE_SIG_cross_rsdpg_128_fast
    OQS_SIGALG_ENTRY(CROSSrsdpg128fast, CROSSrsdpg128fast, CROSSrsdpg128fast, "1.3.6.1.4.1.62245.2.1.11.2", 73),
#endif
#ifdef OQS_ENABLE_SIG_cross_rsdpg_128_small
    OQS_SIGALG_ENTRY(CROSSrsdpg128small, CROSSrsdpg128small, CROSSrsdpg128small, "1.3.6.1.4.1.62245.2.1.12.2", 74),
#endif
#ifdef OQS_ENABLE_SIG_cross_rsdpg_192_balanced
    OQS_SIGALG_ENTRY(CROSSrsdpg192balanced, CROSSrsdpg192balanced, CROSSrsdpg192balanced, "1.3.6.1.4.1.62245.2.1.13.2", 75),
#endif
#ifdef OQS_ENABLE_SIG_cross_rsdpg_192_fast
    OQS_SIGALG_ENTRY(CROSSrsdpg192fast, CROSSrsdpg192fast, CROSSrsdpg192fast, "1.3.6.1.4.1.62245.2.1.14.2", 76),
#endif
#ifdef OQS_ENABLE_SIG_cross_rsdpg_192_small
    OQS_SIGALG_ENTRY(CROSSrsdpg192small, CROSSrsdpg192small, CROSSrsdpg192small, "1.3.6.1.4.1.62245.2.1.15.2", 77),
#endif
#ifdef OQS_ENABLE_SIG_cross_rsdpg_256_balanced
    OQS_SIGALG_ENTRY(CROSSrsdpg256balanced, CROSSrsdpg256balanced, CROSSrsdpg256balanced, "1.3.6.1.4.1.62245.2.1.16.2", 78),
#endif
#ifdef OQS_ENABLE_SIG_cross_rsdpg_256_fast
    OQS_SIGALG_ENTRY(CROSSrsdpg256fast, CROSSrsdpg256fast, CROSSrsdpg256fast, "1.3.6.1.4.1.62245.2.1.17.2", 79),
#endif
#ifdef OQS_ENABLE_SIG_cross_rsdpg_256_small
    OQS_SIGALG_ENTRY(CROSSrsdpg256small, CROSSrsdpg256small, CROSSrsdpg256small, "1.3.6.1.4.1.62245.2.1.18.2", 80),
#endif
#ifdef OQS_ENABLE_SIG_uov_ov_Ip_pkc
    OQS_SIGALG_ENTRY(OV_Ip_pkc, OV_Ip_pkc, OV_Ip_pkc, "1.3.9999.9.6.1", 81),
    OQS_SIGALG_ENTRY(p256_OV_Ip_pkc, p256_OV_Ip_pkc, p256_OV_Ip_pkc, "1.3.9999.9.6.2", 82),
#endif
#ifdef OQS_ENABLE_SIG_uov_ov_Ip_pkc_skc
    OQS_SIGALG_ENTRY(OV_Ip_pkc_skc, OV_Ip_pkc_skc, OV_Ip_pkc_skc, "1.3.9999.9.10.1", 83),
    OQS_SIGALG_ENTRY(p256_OV_Ip_pkc_skc, p256_OV_Ip_pkc_skc, p256_OV_Ip_pkc_skc, "1.3.9999.9.10.2", 84),
#endif
#ifdef OQS_ENABLE_SIG_snova_SNOVA_24_5_4
    OQS_SIGALG_ENTRY(snova2454, snova2454, snova2454, "1.3.9999.10.1.1", 85),
    OQS_SIGALG_ENTRY(p256_snova2454, p256_snova2454, p256_snova2454, "1.3.9999.10.1.2", 86),
#endif
#ifdef OQS_ENABLE_SIG_snova_SNOVA_24_5_4_esk
    OQS_SIGALG_ENTRY(snova2454esk, snova2454esk, snova2454esk, "1.3.9999.10.3.1", 87),
    OQS_SIGALG_ENTRY(p256_snova2454esk, p256_snova2454esk, p256_snova2454esk, "1.3.9999.10.3.2", 88),
#endif
#ifdef OQS_ENABLE_SIG_snova_SNOVA_37_17_2
    OQS_SIGALG_ENTRY(snova37172, snova37172, snova37172, "1.3.9999.10.5.1", 89),
    OQS_SIGALG_ENTRY(p256_snova37172, p256_snova37172, p256_snova37172, "1.3.9999.10.5.2", 90),
#endif
#ifdef OQS_ENABLE_SIG_snova_SNOVA_24_5_5
    OQS_SIGALG_ENTRY(snova2455, snova2455, snova2455, "1.3.9999.10.10.1", 91),
    OQS_SIGALG_ENTRY(p384_snova2455, p384_snova2455, p384_snova2455, "1.3.9999.10.10.2", 92),
#endif
#ifdef OQS_ENABLE_SIG_snova_SNOVA_29_6_5
    OQS_SIGALG_ENTRY(snova2965, snova2965, snova2965, "1.3.9999.10.12.1", 93),
    OQS_SIGALG_ENTRY(p521_snova2965, p521_snova2965, p521_snova2965, "1.3.9999.10.12.2", 94),
#endif
///// OQS_TEMPLATE_FRAGMENT_SIGALG_NAMES_END
};

static int oqs_sigalg_capability(OSSL_CALLBACK *cb, void *arg) {
    size_t i;

    // relaxed assertion for the case that not all algorithms are enabled in
    // liboqs:
    assert(OSSL_NELEM(oqs_param_sigalg_list) <= OSSL_NELEM(oqs_sigalg_list));
    for (i = 0; i < OSSL_NELEM(oqs_param_sigalg_list); i++) {
        // do not register algorithms disabled at runtime
        if (sk_OPENSSL_STRING_find(oqsprov_get_rt_disabled_algs(),
                                   (char *)oqs_param_sigalg_list[i][1].data) <
            0) {
            if (!cb(oqs_param_sigalg_list[i], arg))
                return 0;
        }
    }

    return 1;
}
#endif /* OSSL_CAPABILITY_TLS_SIGALG_NAME */

int oqs_provider_get_capabilities(void *provctx, const char *capability,
                                  OSSL_CALLBACK *cb, void *arg) {
    if (strcasecmp(capability, "TLS-GROUP") == 0)
        return oqs_group_capability(cb, arg);

#ifdef OSSL_CAPABILITY_TLS_SIGALG_NAME
    if (strcasecmp(capability, "TLS-SIGALG") == 0)
        return oqs_sigalg_capability(cb, arg);
#else
#ifndef NDEBUG
    fprintf(stderr, "Warning: OSSL_CAPABILITY_TLS_SIGALG_NAME not defined: "
                    "OpenSSL version used that does not support pluggable "
                    "signature capabilities.\nUpgrading OpenSSL installation "
                    "recommended to enable QSC TLS signature support.\n\n");
#endif /* NDEBUG */
#endif /* OSSL_CAPABILITY_TLS_SIGALG_NAME */

    /* We don't support this capability */
    return 0;
}
