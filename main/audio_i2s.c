/*
 * I2S audio output driver for ESP-IDF v6.0+
 *
 * Uses the new I2S standard-mode driver (driver/i2s_std.h).
 * Pin assignments and optional MCLK / MUTE are configured via Kconfig
 * (see Kconfig.projbuild in the same directory).
 *
 * Copyright (c) 2024
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <string.h>
#include "audio.h"
#include "driver/i2s_std.h"
#include "esp_log.h"

#if CONFIG_I2S_MUTE_ENABLED
#include "driver/gpio.h"
#endif

static const char *TAG = "audio_i2s";

static i2s_chan_handle_t tx_handle;

/* ---------- helpers ------------------------------------------------------ */

#if CONFIG_I2S_MUTE_ENABLED
static void mute_set(int muted)
{
    gpio_set_level((gpio_num_t)CONFIG_I2S_MUTE_GPIO, muted ? 0 : 1);
}
#endif

/* ---------- audio_output callbacks --------------------------------------- */

static int i2s_init(int argc, char **argv)
{
    /* ---- channel allocation ---- */
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(
        I2S_NUM_AUTO, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num  = CONFIG_I2S_DMA_DESC_NUM;
    chan_cfg.dma_frame_num = CONFIG_I2S_DMA_FRAME_NUM;

    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle, NULL));

    /* ---- standard-mode configuration ---- */
    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(44100),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
                        I2S_DATA_BIT_WIDTH_16BIT,
                        I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
#if CONFIG_I2S_MCLK_ENABLED
            .mclk = (gpio_num_t)CONFIG_I2S_MCLK_GPIO,
#else
            .mclk = I2S_GPIO_UNUSED,
#endif
            .bclk = (gpio_num_t)CONFIG_I2S_BCLK_GPIO,
            .ws   = (gpio_num_t)CONFIG_I2S_WS_GPIO,
            .dout = (gpio_num_t)CONFIG_I2S_DOUT_GPIO,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(tx_handle));

    /* ---- optional MUTE pin ---- */
#if CONFIG_I2S_MUTE_ENABLED
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << CONFIG_I2S_MUTE_GPIO,
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    mute_set(1);   /* start muted */
#endif

    ESP_LOGI(TAG, "I2S output initialised  BCLK=%d  WS=%d  DOUT=%d"
#if CONFIG_I2S_MCLK_ENABLED
             "  MCLK=%d"
#endif
#if CONFIG_I2S_MUTE_ENABLED
             "  MUTE=%d"
#endif
             ,
             CONFIG_I2S_BCLK_GPIO,
             CONFIG_I2S_WS_GPIO,
             CONFIG_I2S_DOUT_GPIO
#if CONFIG_I2S_MCLK_ENABLED
             , CONFIG_I2S_MCLK_GPIO
#endif
#if CONFIG_I2S_MUTE_ENABLED
             , CONFIG_I2S_MUTE_GPIO
#endif
    );

    return 0;
}

static void i2s_deinit(void)
{
    if (tx_handle) {
        i2s_channel_disable(tx_handle);
        i2s_del_channel(tx_handle);
        tx_handle = NULL;
    }
#if CONFIG_I2S_MUTE_ENABLED
    mute_set(1);
#endif
}

static void i2s_start(int sample_rate)
{
    /* Reconfigure the clock if the sample rate changed. */
    i2s_channel_disable(tx_handle);

    i2s_std_clk_config_t clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate);
    ESP_ERROR_CHECK(i2s_channel_reconfig_std_clock(tx_handle, &clk_cfg));

    ESP_ERROR_CHECK(i2s_channel_enable(tx_handle));

#if CONFIG_I2S_MUTE_ENABLED
    mute_set(0);   /* un-mute */
#endif

    ESP_LOGI(TAG, "I2S playback started at %d Hz", sample_rate);
}

static void i2s_play(short buf[], int samples)
{
    size_t bytes_written = 0;
    /* Each stereo sample is 4 bytes (2 × 16-bit). */
    size_t bytes_to_write = (size_t)samples * 4;
    i2s_channel_write(tx_handle, buf, bytes_to_write,
                      &bytes_written, portMAX_DELAY);
}

static void i2s_stop(void)
{
#if CONFIG_I2S_MUTE_ENABLED
    mute_set(1);
#endif
    ESP_LOGI(TAG, "I2S playback stopped");
}

static void i2s_help(void)
{
    printf("    I2S output — pin configuration is set via menuconfig.\n");
}

audio_output audio_i2s = {
    .name   = "i2s",
    .help   = &i2s_help,
    .init   = &i2s_init,
    .deinit = &i2s_deinit,
    .start  = &i2s_start,
    .stop   = &i2s_stop,
    .play   = &i2s_play,
    .volume = NULL,
};
