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

#define LED_COUNT 6

// Brightness 0–255 (try 10–40 indoors)
#define BRIGHTNESS 20


// Button stuff
#define BTN_CE     7
#define BTN_CLK    6
#define BTN_DATA   4
#define BTN_LOAD   5

static led_strip_handle_t strip;


static void buttons_init(void)
{
    gpio_config_t out = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask =
            (1ULL << BTN_CLK) |
            (1ULL << BTN_CE)  |
            (1ULL << BTN_LOAD),
    };
    gpio_config(&out);

    gpio_config_t in = {
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << BTN_DATA),
        .pull_up_en = 1,
    };
    gpio_config(&in);

    // Idle states
    gpio_set_level(BTN_CE, 1);
    gpio_set_level(BTN_CLK, 0);
    gpio_set_level(BTN_LOAD, 1);
}

static uint8_t buttons_read(void)
{
    uint8_t data = 0;

    // Disable clock
    gpio_set_level(BTN_CE, 1);

    // Latch inputs
    gpio_set_level(BTN_LOAD, 0);
    esp_rom_delay_us(5);
    gpio_set_level(BTN_LOAD, 1);

    // Enable clock
    gpio_set_level(BTN_CE, 0);

    for (int i = 0; i < 8; i++) {
        int value = gpio_get_level(BTN_DATA);

        if (value) {
            data |= (1 << i);
        }

        // Clock pulse
        gpio_set_level(BTN_CLK, 1);
        esp_rom_delay_us(1);
        gpio_set_level(BTN_CLK, 0);
    }

    // Disable clock again
    gpio_set_level(BTN_CE, 1);

    return data;
}


// Scale helper
static uint8_t scale(uint8_t value)
{
    return (value * BRIGHTNESS) / 255;
}


static void ws2812_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED_ENABLE_GPIO),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io_conf);
    gpio_set_level(LED_ENABLE_GPIO, 1);

    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_GPIO,
        .max_leds = LED_COUNT,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags.invert_out = false,
    };

    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000,
        .mem_block_symbols = 64,
        .flags.with_dma = false,
    };

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &strip));
    led_strip_clear(strip);
}


static void set_all(uint8_t r, uint8_t g, uint8_t b)
{
    r = scale(r);
    g = scale(g);
    b = scale(b);

    for (int i = 0; i < LED_COUNT; i++) {
        led_strip_set_pixel(strip, i, r, g, b);
    }

    led_strip_refresh(strip);
}


void app_main(void)
{
    ws2812_init();
    buttons_init();

    uint8_t last = 0;

    while (1) {

        uint8_t btn = buttons_read();

        if (btn != last) {

            if (btn & (1 << 0)) set_all(255, 0, 0);
            if (btn & (1 << 1)) set_all(0, 255, 0);
            if (btn & (1 << 2)) set_all(0, 0, 255);
            if (btn & (1 << 3)) set_all(255, 255, 255);
            if (btn & (1 << 4)) set_all(255, 0, 255);
            if (btn & (1 << 5)) set_all(0, 255, 255);
            if (btn & (1 << 6)) set_all(255, 128, 0);
            if (btn & (1 << 7)) set_all(0, 0, 0);

            last = btn;
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
