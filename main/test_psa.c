#include <stdio.h>
#include <psa/crypto.h>
#include <mbedtls/build_info.h>
#include <mbedtls/pk.h>
void test_me() {
    mbedtls_pk_context ctx;
    mbedtls_svc_key_id_t id = ctx.MBEDTLS_PRIVATE(priv_id);
}
