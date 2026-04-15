import sys
content = open('D:/Harry/shairport-esp32/main/common.c', 'r').read()

import re
new_content = re.sub(
    r'  mbedtls_entropy_context entropy;\n  mbedtls_ctr_drbg_context ctr_drbg;\n\n  mbedtls_entropy_init\(&entropy\);\n\n  mbedtls_ctr_drbg_init\(&ctr_drbg\);\n  mbedtls_ctr_drbg_seed\(&ctr_drbg, mbedtls_entropy_func, &entropy, \(const unsigned char \*\)pers,\n                        strlen\(pers\)\);\n\n  mbedtls_pk_init\(&pkctx\);\n\n#if MBEDTLS_VERSION_MAJOR == 3\n  rc = mbedtls_pk_parse_key\(&pkctx, \(unsigned char \*\)super_secret_key, sizeof\(super_secret_key\),\n                            NULL, 0, mbedtls_ctr_drbg_random, &ctr_drbg\);\n#else\n  rc = mbedtls_pk_parse_key\(&pkctx, \(unsigned char \*\)super_secret_key, sizeof\(super_secret_key\),\n                            NULL, 0\);\n\n#endif',
    r'''
  mbedtls_pk_init(&pkctx);
#if MBEDTLS_VERSION_MAJOR < 4
  mbedtls_entropy_context entropy;
  mbedtls_ctr_drbg_context ctr_drbg;
  mbedtls_entropy_init(&entropy);
  mbedtls_ctr_drbg_init(&ctr_drbg);
  mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, (const unsigned char *)pers, strlen(pers));
#if MBEDTLS_VERSION_MAJOR == 3
  rc = mbedtls_pk_parse_key(&pkctx, (unsigned char *)super_secret_key, sizeof(super_secret_key),
                            NULL, 0, mbedtls_ctr_drbg_random, &ctr_drbg);
#else
  rc = mbedtls_pk_parse_key(&pkctx, (unsigned char *)super_secret_key, sizeof(super_secret_key), NULL, 0);
#endif
#else
  rc = mbedtls_pk_parse_key(&pkctx, (unsigned char *)super_secret_key, sizeof(super_secret_key), NULL, 0, NULL, NULL);
#endif
''',
    content,
    flags=re.MULTILINE
)

new_content = re.sub(
    r'    mbedtls_ctr_drbg_free\(&ctr_drbg\);\n    mbedtls_entropy_free\(&entropy\);\n',
    r'''#if MBEDTLS_VERSION_MAJOR < 4
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
#endif\n''',
    new_content
)

open('D:/Harry/shairport-esp32/main/common.c', 'w').write(new_content)
