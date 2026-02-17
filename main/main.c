#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "led_strip.h"
#include "esp_log.h"
#include "esp_random.h"
#include <math.h>

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


static int current_mode = 0;
static uint32_t anim_step = 0;

static led_strip_handle_t strip;


// Scale helper
static uint8_t scale(uint8_t value)
{
    return (value * BRIGHTNESS) / 255;
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

static void hsv2rgb(uint16_t h, uint8_t s, uint8_t v,
                    uint8_t *r, uint8_t *g, uint8_t *b)
{
    uint8_t region = h / 43;
    uint8_t remainder = (h - (region * 43)) * 6;

    uint8_t p = (v * (255 - s)) >> 8;
    uint8_t q = (v * (255 - ((s * remainder) >> 8))) >> 8;
    uint8_t t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

    switch (region) {
        case 0: *r=v; *g=t; *b=p; break;
        case 1: *r=q; *g=v; *b=p; break;
        case 2: *r=p; *g=v; *b=t; break;
        case 3: *r=p; *g=q; *b=v; break;
        case 4: *r=t; *g=p; *b=v; break;
        default:*r=v; *g=p; *b=q; break;
    }
}

static uint8_t heat[LED_COUNT];

static void anim_fire(void)
{
    for (int i = 0; i < LED_COUNT; i++) {
        int cooldown = esp_random() % 40;
        heat[i] = (heat[i] > cooldown) ? heat[i] - cooldown : 0;
    }

    for (int i = LED_COUNT - 1; i >= 2; i--) {
        heat[i] = (heat[i-1] + heat[i-2] + heat[i-2]) / 3;
    }

    if ((esp_random() % 255) < 120) {
        int y = esp_random() % 2; // bottom LEDs
        heat[y] = 160 + (esp_random() % 95);
    }

    for (int i = 0; i < LED_COUNT; i++) {
        uint8_t t = heat[i];

        uint8_t r = t;
        uint8_t g = t > 128 ? 255 : t * 2;
        uint8_t b = t > 200 ? t : 0;

        led_strip_set_pixel(strip, i,
                            scale(r),
                            scale(g),
                            scale(b));
    }

    led_strip_refresh(strip);
}


static void anim_rainbow(void)
{
    for (int i = 0; i < LED_COUNT; i++) {

        uint16_t hue = (anim_step * 5 + i * 40) % 255;

        uint8_t r,g,b;
        hsv2rgb(hue, 255, 255, &r, &g, &b);

        led_strip_set_pixel(strip, i,
                            scale(r),
                            scale(g),
                            scale(b));
    }

    led_strip_refresh(strip);
}

static void anim_police(void)
{
    bool phase = (anim_step / 10) % 2;

    for (int i = 0; i < LED_COUNT; i++) {

        if ((i % 2) == phase)
            led_strip_set_pixel(strip, i, scale(255), 0, 0);
        else
            led_strip_set_pixel(strip, i, 0, 0, scale(255));
    }

    led_strip_refresh(strip);
}

static void anim_breath(void)
{
    float x = (sinf(anim_step * 0.05f) + 1.0f) * 0.5f;
    uint8_t v = x * 255;

    set_all(v, v, v);
}

static void anim_sparkle(void)
{
    set_all(10, 10, 10);

    int p = esp_random() % LED_COUNT;

    led_strip_set_pixel(strip, p,
                        scale(255),
                        scale(255),
                        scale(255));

    led_strip_refresh(strip);
}

static void anim_lava(void)
{
    float x = (sinf(anim_step * 0.08f) + 1.0f) * 0.5f;
    uint8_t v = x * 255;

    for (int i = 0; i < LED_COUNT; i++) {

        led_strip_set_pixel(strip, i,
                            scale(v),
                            scale(v / 4),
                            0);
    }

    led_strip_refresh(strip);
}

static void anim_wipe(void)
{
    int pos = anim_step % LED_COUNT;

    for (int i = 0; i < LED_COUNT; i++) {

        if (i == pos)
            led_strip_set_pixel(strip, i, scale(255), scale(100), 0);
        else
            led_strip_set_pixel(strip, i, 0, 0, 0);
    }

    led_strip_refresh(strip);
}


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


static void run_animation(void)
{
    switch (current_mode) {

        case 0: anim_rainbow(); break;
        case 1: anim_fire(); break;
        case 2: anim_police(); break;
        case 3: anim_breath(); break;
        case 4: anim_wipe(); break;
        case 5: anim_sparkle(); break;
        case 6: anim_lava(); break;
        case 7: set_all(0,0,0); break;
    }

    anim_step++;
}



void app_main(void)
{
    ws2812_init();
    buttons_init();

    uint8_t last = 0;

    while (1) {

        uint8_t btn = buttons_read();

        for (int i = 0; i < 8; i++) {
        if (btn & (1 << i)) {
            current_mode = i;
         }
        }


        run_animation();

        vTaskDelay(pdMS_TO_TICKS(30));
    }
}
