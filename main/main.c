#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "led_strip.h"
#include "esp_log.h"

#define TAG "WS2812"

// Pins
#define LED_GPIO        48
#define LED_ENABLE_GPIO 47

// Number of LEDs
#define LED_COUNT 1

static led_strip_handle_t strip;

static void ws2812_init(void)
{
    // Enable pin HIGH
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED_ENABLE_GPIO),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io_conf);
    gpio_set_level(LED_ENABLE_GPIO, 1);

    // LED strip configuration
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_GPIO,
        .max_leds = LED_COUNT,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags.invert_out = false,
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000, // 10 MHz
        .mem_block_symbols = 64,
        .flags.with_dma = false,
    };

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &strip));

    led_strip_clear(strip);
}

static void set_color(uint8_t r, uint8_t g, uint8_t b)
{
    led_strip_set_pixel(strip, 0, r, g, b);
    led_strip_refresh(strip);
}

void app_main(void)
{
    ws2812_init();

    ESP_LOGI(TAG, "Starting WS2812 demo");

    while (1) {
        set_color(255, 0, 0);
        vTaskDelay(pdMS_TO_TICKS(1000));

        set_color(0, 255, 0);
        vTaskDelay(pdMS_TO_TICKS(1000));

        set_color(0, 0, 255);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

