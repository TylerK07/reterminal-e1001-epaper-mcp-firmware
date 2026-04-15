#include "buttons.h"
#include <string.h>
#include "board.h"
#ifdef ESP_PLATFORM
#include "driver/gpio.h"
#endif

static board_pin_map_t g_pin_map;
static bool g_buttons_ready = false;

error_code_t buttons_init(void) {
    if (board_get_pin_map(&g_pin_map) != ERR_OK) {
        return ERR_INTERNAL;
    }

#ifdef ESP_PLATFORM
    gpio_config_t cfg;

    memset(&cfg, 0, sizeof(cfg));
    cfg.pin_bit_mask =
        (1ULL << (uint32_t)g_pin_map.key0_gpio) |
        (1ULL << (uint32_t)g_pin_map.key1_gpio) |
        (1ULL << (uint32_t)g_pin_map.key2_gpio);
    cfg.mode = GPIO_MODE_INPUT;
    cfg.pull_up_en = GPIO_PULLUP_ENABLE;
    cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    cfg.intr_type = GPIO_INTR_DISABLE;

    if (gpio_config(&cfg) != ESP_OK) {
        return ERR_INTERNAL;
    }
#endif

    g_buttons_ready = true;
    return ERR_OK;
}

error_code_t buttons_get_state(button_state_t *out_state) {
    if (!out_state) {
        return ERR_INVALID_ARGS;
    }
    if (!g_buttons_ready) {
        return ERR_INTERNAL;
    }

    memset(out_state, 0, sizeof(*out_state));

#ifdef ESP_PLATFORM
    /* Buttons are treated as active-low inputs with pull-ups for boot override. */
    out_state->key0_pressed = (gpio_get_level((gpio_num_t)g_pin_map.key0_gpio) == 0);
    out_state->key1_pressed = (gpio_get_level((gpio_num_t)g_pin_map.key1_gpio) == 0);
    out_state->key2_pressed = (gpio_get_level((gpio_num_t)g_pin_map.key2_gpio) == 0);
#endif

    return ERR_OK;
}

error_code_t buttons_is_any_pressed(bool *out_pressed) {
    button_state_t state;
    error_code_t err;

    if (!out_pressed) {
        return ERR_INVALID_ARGS;
    }

    err = buttons_get_state(&state);
    if (err != ERR_OK) {
        return err;
    }

    *out_pressed = state.key0_pressed || state.key1_pressed || state.key2_pressed;
    return ERR_OK;
}
