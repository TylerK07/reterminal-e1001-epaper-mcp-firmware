#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "errors.h"

typedef struct {
    bool sensor_present;
    bool serial_number_valid;
    bool reading_valid;
    error_code_t last_serial_error;
    error_code_t last_measurement_error;
    uint8_t i2c_bus_index;
    uint8_t i2c_address;
    uint8_t serial_raw[6];
    uint8_t measurement_raw[6];
    uint32_t serial_number;
    int16_t temperature_centi_c;
    uint16_t humidity_centi_pct;
} platform_environment_status_t;

error_code_t environment_sensor_init(void);
error_code_t environment_sensor_get_status(platform_environment_status_t *out_status);
