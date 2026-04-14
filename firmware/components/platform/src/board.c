#include "board.h"

static const board_pin_map_t g_board_pin_map = {
    .screen_sck_gpio = 7,
    .screen_miso_gpio = 8,
    .screen_mosi_gpio = 9,
    .screen_cs_gpio = 10,
    .screen_dc_gpio = 11,
    .screen_reset_gpio = 12,
    .screen_busy_gpio = 13,

    .sd_sck_gpio = 7,
    .sd_miso_gpio = 8,
    .sd_mosi_gpio = 9,
    .sd_cs_gpio = 14,
    .sd_detect_gpio = 15,
    .sd_enable_gpio = 16,

    .key0_gpio = 3,
    .key1_gpio = 4,
    .key2_gpio = 5,
    .status_led_gpio = 6,

    .battery_adc_gpio = 1,
    .battery_enable_gpio = 21,

    .i2c0_sda_gpio = 19,
    .i2c0_scl_gpio = 20,
    .i2c1_sda_gpio = 39,
    .i2c1_scl_gpio = 40,

    .uart1_tx_gpio = 17,
    .uart1_rx_gpio = 18,

    .pdm_enable_gpio = 38,
    .pdm_data_gpio = 41,
    .pdm_clk_gpio = 42,
    .buzzer_enable_gpio = 45,

    .touch_int_gpio = 47,
    .touch_reset_gpio = 48,
    .expansion_gpio0 = 46,
    .expansion_gpio1 = 2,
};

static const board_i2c_device_map_t g_board_i2c_device_map = {
    .rtc_i2c_address = 0x51,
    .temperature_sensor_i2c_address = 0x44,
};

static const board_display_profile_t g_board_display_profile = {
    .panel_model = "GDEY075T7",
    .controller = "UC8179",
    .width = 800U,
    .height = 480U,
    .supports_partial_refresh = true,
    .supports_fast_refresh = true,
    .full_refresh_time_ms = 3000U,
    .fast_refresh_time_ms = 1500U,
    .partial_refresh_time_ms = 300U,
};

error_code_t board_get_pin_map(board_pin_map_t *out_pin_map) {
    if (!out_pin_map) {
        return ERR_INVALID_ARGS;
    }

    *out_pin_map = g_board_pin_map;
    return ERR_OK;
}

error_code_t board_get_i2c_device_map(board_i2c_device_map_t *out_device_map) {
    if (!out_device_map) {
        return ERR_INVALID_ARGS;
    }

    *out_device_map = g_board_i2c_device_map;
    return ERR_OK;
}

error_code_t board_get_display_profile(board_display_profile_t *out_display_profile) {
    if (!out_display_profile) {
        return ERR_INVALID_ARGS;
    }

    *out_display_profile = g_board_display_profile;
    return ERR_OK;
}

const char *board_get_name(void) {
    return "Seeed reTerminal E1001";
}
