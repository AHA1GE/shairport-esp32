import sys
content = sys.stdin.read()
import re
new_content = re.sub(
    r'  uint8_t \*outbuf = NULL;\n  trsa = mbedtls_pk_rsa\(pkctx\);\n\n    switch \(mode\) \{.* ESP_LOGE\("rsa_apply", "bad rsa mode"\);\n    \}',
    r'''  uint8_t *outbuf = NULL;
#if MBEDTLS_VERSION_MAJOR >= 4
  mbedtls_svc_key_id_t key_id = pkctx.MBEDTLS_PRIVATE(priv_id);
  psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;
  mbedtls_pk_get_psa_attributes(&pkctx, &attr);
  size_t bits = psa_get_key_bits(&attr);
  size_t outbuf_sz = bits / 8;
  outbuf = malloc(outbuf_sz);
  size_t olen = 0;
  psa_status_t status = PSA_SUCCESS;

    switch (mode) {
        case RSA_MODE_AUTH:
            status = psa_asymmetric_encrypt(key_id, PSA_ALG_RSA_PKCS1V15_CRYPT, input, inlen, NULL, 0, outbuf, outbuf_sz, &olen);
            if (status != PSA_SUCCESS) ESP_LOGE("rsa_apply", "mbedtls_pk_encrypt error %d.", (int)status);
            *outlen = (int)outbuf_sz;
            break;
        case RSA_MODE_KEY:
            status = psa_asymmetric_decrypt(key_id, PSA_ALG_RSA_OAEP(PSA_ALG_SHA_1), input, inlen, NULL, 0, outbuf, outbuf_sz, &olen);
            if (status != PSA_SUCCESS) ESP_LOGE("rsa_apply", "mbedtls_pk_decrypt error %d.", (int)status);
            *outlen = olen;
            break;
        default:
            ESP_LOGE("rsa_apply", "bad rsa mode");
    }
  psa_reset_key_attributes(&attr);
#else
  trsa = mbedtls_pk_rsa(pkctx);

    switch (mode) {
        case RSA_MODE_AUTH:
            mbedtls_rsa_set_padding(trsa, MBEDTLS_RSA_PKCS_V15, MBEDTLS_MD_NONE);
            outbuf = malloc(mbedtls_rsa_get_len(trsa));
#if MBEDTLS_VERSION_MAJOR == 3
            rc = mbedtls_rsa_pkcs1_encrypt(trsa, mbedtls_ctr_drbg_random, &ctr_drbg,
                                           inlen, input, outbuf);
#else
            rc = mbedtls_rsa_pkcs1_encrypt(trsa, mbedtls_ctr_drbg_random, &ctr_drbg, MBEDTLS_RSA_PRIVATE,
                                   inlen, input, outbuf);
#endif
            if (rc != 0)
                ESP_LOGE("rsa_apply", "mbedtls_pk_encrypt error %d.", rc);
            *outlen = (int)mbedtls_rsa_get_len(trsa);
            break;
        case RSA_MODE_KEY:
            mbedtls_rsa_set_padding(trsa, MBEDTLS_RSA_PKCS_V21, MBEDTLS_MD_SHA1);
            outbuf = malloc(mbedtls_rsa_get_len(trsa));
#if MBEDTLS_VERSION_MAJOR == 3
            rc = mbedtls_rsa_pkcs1_decrypt(trsa, mbedtls_ctr_drbg_random, &ctr_drbg,
                                           &olen, input, outbuf, mbedtls_rsa_get_len(trsa));
#else
            rc = mbedtls_rsa_pkcs1_decrypt(trsa, mbedtls_ctr_drbg_random, &ctr_drbg, MBEDTLS_RSA_PRIVATE,
                                   &olen, input, outbuf, trsa->len);
#endif
            if (rc != 0)
                ESP_LOGE("rsa_apply", "mbedtls_pk_decrypt error %d.", rc);
            *outlen = olen;
            break;
        default:
            ESP_LOGE("rsa_apply", "bad rsa mode");
    }
#endif''',
    content,
    flags=re.DOTALL
)

with open('D:/Harry/shairport-esp32/main/common.c', 'w') as f:
    f.write(new_content)
