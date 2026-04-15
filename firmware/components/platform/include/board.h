#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "errors.h"

typedef struct {
    int16_t screen_sck_gpio;
    int16_t screen_miso_gpio;
    int16_t screen_mosi_gpio;
    int16_t screen_cs_gpio;
    int16_t screen_dc_gpio;
    int16_t screen_reset_gpio;
    int16_t screen_busy_gpio;

    int16_t sd_sck_gpio;
    int16_t sd_miso_gpio;
    int16_t sd_mosi_gpio;
    int16_t sd_cs_gpio;
    int16_t sd_detect_gpio;
    int16_t sd_enable_gpio;

    int16_t key0_gpio;
    int16_t key1_gpio;
    int16_t key2_gpio;
    int16_t status_led_gpio;

    int16_t battery_adc_gpio;
    int16_t battery_enable_gpio;

    int16_t i2c0_sda_gpio;
    int16_t i2c0_scl_gpio;
    int16_t i2c1_sda_gpio;
    int16_t i2c1_scl_gpio;

    int16_t uart1_tx_gpio;
    int16_t uart1_rx_gpio;

    int16_t pdm_enable_gpio;
    int16_t pdm_data_gpio;
    int16_t pdm_clk_gpio;
    int16_t buzzer_enable_gpio;

    int16_t touch_int_gpio;
    int16_t touch_reset_gpio;
    int16_t expansion_gpio0;
    int16_t expansion_gpio1;
} board_pin_map_t;

typedef struct {
    uint8_t rtc_i2c_address;
    uint8_t temperature_sensor_i2c_address;
} board_i2c_device_map_t;

typedef struct {
    char panel_model[32];
    char controller[16];
    uint16_t width;
    uint16_t height;
    bool supports_partial_refresh;
    bool supports_fast_refresh;
    uint16_t full_refresh_time_ms;
    uint16_t fast_refresh_time_ms;
    uint16_t partial_refresh_time_ms;
} board_display_profile_t;

error_code_t board_get_pin_map(board_pin_map_t *out_pin_map);
error_code_t board_get_i2c_device_map(board_i2c_device_map_t *out_device_map);
error_code_t board_get_display_profile(board_display_profile_t *out_display_profile);
const char *board_get_name(void);
