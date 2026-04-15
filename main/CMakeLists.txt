idf_component_register(SRCS "main.c" "shairport.c" "common.c" "rtsp.c" "shairport_mdns.c" "rtp.c" "player.c" "alac.c" "audio.c" "audio_dummy.c" "audio_i2s.c"
        INCLUDE_DIRS "."
        REQUIRES mbedtls nvs_flash esp_driver_i2s protocol_examples_common)