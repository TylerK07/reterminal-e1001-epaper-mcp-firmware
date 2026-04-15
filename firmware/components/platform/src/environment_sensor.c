#include "environment_sensor.h"
#include <string.h>
#include "board.h"

#ifdef ESP_PLATFORM
#include "driver/i2c.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif

static board_pin_map_t g_pin_map;
static board_i2c_device_map_t g_device_map;
static bool g_i2c_ready = false;
static bool g_sensor_present = false;
static bool g_serial_number_valid = false;
static uint32_t g_serial_number = 0U;
static uint8_t g_sensor_i2c_address = 0U;
static error_code_t g_last_serial_error = ERR_NOT_FOUND;
static error_code_t g_last_measurement_error = ERR_NOT_FOUND;
static uint8_t g_last_serial_raw[6];
static uint8_t g_last_measurement_raw[6];
static bool g_last_reading_valid = false;
static int16_t g_last_temperature_centi_c = 0;
static uint16_t g_last_humidity_centi_pct = 0U;

#ifdef ESP_PLATFORM
static const uint8_t ENVIRONMENT_SENSOR_I2C_BUS_INDEX = 0U;
static const uint32_t ENVIRONMENT_SENSOR_I2C_FREQ_HZ = 100000U;
static const TickType_t ENVIRONMENT_SENSOR_TIMEOUT_TICKS = pdMS_TO_TICKS(100);
static const uint8_t SHT4X_CMD_MEASURE_HIGH_PRECISION = 0xFDU;
static const uint8_t SHT4X_CMD_READ_SERIAL = 0x89U;
static const uint8_t SHT4X_CMD_SOFT_RESET = 0x94U;
static const uint32_t SHT4X_MEASUREMENT_DELAY_MS = 20U;
static const uint32_t SHT4X_MEASUREMENT_RETRY_COUNT = 3U;

static uint8_t environment_sensor_crc8(const uint8_t *data, uint32_t len) {
    uint8_t crc = 0xFFU;
    uint32_t i;
    uint32_t bit;

    for (i = 0U; i < len; ++i) {
        crc ^= data[i];
        for (bit = 0U; bit < 8U; ++bit) {
            if ((crc & 0x80U) != 0U) {
                crc = (uint8_t)((crc << 1U) ^ 0x31U);
            } else {
                crc <<= 1U;
            }
        }
    }

    return crc;
}

static error_code_t environment_sensor_write_command(uint8_t command) {
    i2c_cmd_handle_t cmd;
    esp_err_t err;

    cmd = i2c_cmd_link_create();
    if (!cmd) {
        return ERR_INTERNAL;
    }

    err = i2c_master_start(cmd);
    if (err == ESP_OK) {
        err = i2c_master_write_byte(cmd, (uint8_t)((g_sensor_i2c_address << 1U) | I2C_MASTER_WRITE), true);
    }
    if (err == ESP_OK) {
        err = i2c_master_write_byte(cmd, command, true);
    }
    if (err == ESP_OK) {
        err = i2c_master_stop(cmd);
    }
    if (err == ESP_OK) {
        err = i2c_master_cmd_begin(I2C_NUM_0, cmd, ENVIRONMENT_SENSOR_TIMEOUT_TICKS);
    }
    i2c_cmd_link_delete(cmd);

    return err == ESP_OK ? ERR_OK : ERR_INTERNAL;
}

static error_code_t environment_sensor_read_bytes(uint8_t *buffer, uint32_t len) {
    i2c_cmd_handle_t cmd;
    esp_err_t err;

    if (!buffer || len == 0U) {
        return ERR_INVALID_ARGS;
    }

    memset(buffer, 0, len);

    cmd = i2c_cmd_link_create();
    if (!cmd) {
        return ERR_INTERNAL;
    }

    err = i2c_master_start(cmd);
    if (err == ESP_OK) {
        err = i2c_master_write_byte(cmd, (uint8_t)((g_sensor_i2c_address << 1U) | I2C_MASTER_READ), true);
    }
    if (err == ESP_OK && len > 1U) {
        err = i2c_master_read(cmd, buffer, len - 1U, I2C_MASTER_ACK);
    }
    if (err == ESP_OK) {
        err = i2c_master_read_byte(cmd, &buffer[len - 1U], I2C_MASTER_NACK);
    }
    if (err == ESP_OK) {
        err = i2c_master_stop(cmd);
    }
    if (err == ESP_OK) {
        err = i2c_master_cmd_begin(I2C_NUM_0, cmd, ENVIRONMENT_SENSOR_TIMEOUT_TICKS);
    }
    i2c_cmd_link_delete(cmd);

    return err == ESP_OK ? ERR_OK : ERR_INTERNAL;
}

static error_code_t environment_sensor_probe_address(uint8_t address) {
    i2c_cmd_handle_t cmd;
    esp_err_t err;

    cmd = i2c_cmd_link_create();
    if (!cmd) {
        return ERR_INTERNAL;
    }

    err = i2c_master_start(cmd);
    if (err == ESP_OK) {
        err = i2c_master_write_byte(cmd, (uint8_t)((address << 1U) | I2C_MASTER_WRITE), true);
    }
    if (err == ESP_OK) {
        err = i2c_master_stop(cmd);
    }
    if (err == ESP_OK) {
        err = i2c_master_cmd_begin(I2C_NUM_0, cmd, ENVIRONMENT_SENSOR_TIMEOUT_TICKS);
    }
    i2c_cmd_link_delete(cmd);

    return err == ESP_OK ? ERR_OK : ERR_NOT_FOUND;
}

static error_code_t environment_sensor_ensure_ready(void) {
    i2c_config_t config;

    if (g_i2c_ready) {
        return ERR_OK;
    }

    memset(&config, 0, sizeof(config));
    config.mode = I2C_MODE_MASTER;
    config.sda_io_num = g_pin_map.i2c0_sda_gpio;
    config.scl_io_num = g_pin_map.i2c0_scl_gpio;
    config.sda_pullup_en = GPIO_PULLUP_ENABLE;
    config.scl_pullup_en = GPIO_PULLUP_ENABLE;
    config.master.clk_speed = ENVIRONMENT_SENSOR_I2C_FREQ_HZ;

    if (i2c_param_config(I2C_NUM_0, &config) != ESP_OK) {
        return ERR_INTERNAL;
    }
    if (i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0U, 0U, 0U) != ESP_OK) {
        return ERR_INTERNAL;
    }

    g_i2c_ready = true;
    return ERR_OK;
}

static error_code_t environment_sensor_bind_device(void) {
    static const uint8_t candidate_addresses[] = {0x44U, 0x45U};
    uint32_t address_index;

    if (environment_sensor_ensure_ready() != ERR_OK) {
        return ERR_INTERNAL;
    }

    for (address_index = 0U; address_index < (uint32_t)(sizeof(candidate_addresses) / sizeof(candidate_addresses[0])); ++address_index) {
        if (environment_sensor_probe_address(candidate_addresses[address_index]) == ERR_OK) {
            g_sensor_i2c_address = candidate_addresses[address_index];
            g_sensor_present = true;
            return ERR_OK;
        }
    }

    g_sensor_present = false;
    g_sensor_i2c_address = 0U;
    return ERR_NOT_FOUND;
}

static error_code_t environment_sensor_read_serial_number(void) {
    uint8_t buffer[6];
    uint16_t word0;
    uint16_t word1;
    error_code_t err;

    memset(buffer, 0, sizeof(buffer));

    err = environment_sensor_write_command(SHT4X_CMD_READ_SERIAL);
    if (err != ERR_OK) {
        g_last_serial_error = err;
        memset(g_last_serial_raw, 0, sizeof(g_last_serial_raw));
        return err;
    }
    vTaskDelay(pdMS_TO_TICKS(2));
    err = environment_sensor_read_bytes(buffer, sizeof(buffer));
    memcpy(g_last_serial_raw, buffer, sizeof(g_last_serial_raw));
    if (err != ERR_OK) {
        g_last_serial_error = err;
        return err;
    }
    if (environment_sensor_crc8(buffer, 2U) != buffer[2] || environment_sensor_crc8(&buffer[3], 2U) != buffer[5]) {
        g_last_serial_error = ERR_INTERNAL;
        return ERR_INTERNAL;
    }

    word0 = (uint16_t)(((uint16_t)buffer[0] << 8U) | buffer[1]);
    word1 = (uint16_t)(((uint16_t)buffer[3] << 8U) | buffer[4]);
    g_serial_number = ((uint32_t)word0 << 16U) | word1;
    g_serial_number_valid = true;
    g_sensor_present = true;
    g_last_serial_error = ERR_OK;
    return ERR_OK;
}

static error_code_t environment_sensor_measure(int16_t *out_temperature_centi_c, uint16_t *out_humidity_centi_pct) {
    uint8_t buffer[6];
    uint16_t temp_ticks;
    uint16_t humidity_ticks;
    int32_t humidity_centi;
    error_code_t err = ERR_INTERNAL;
    uint32_t attempt;

    if (!out_temperature_centi_c || !out_humidity_centi_pct) {
        return ERR_INVALID_ARGS;
    }

    for (attempt = 0U; attempt < SHT4X_MEASUREMENT_RETRY_COUNT; ++attempt) {
        memset(buffer, 0, sizeof(buffer));

        err = environment_sensor_write_command(SHT4X_CMD_MEASURE_HIGH_PRECISION);
        if (err != ERR_OK) {
            continue;
        }

        vTaskDelay(pdMS_TO_TICKS(SHT4X_MEASUREMENT_DELAY_MS));
        err = environment_sensor_read_bytes(buffer, sizeof(buffer));
        if (err != ERR_OK) {
            continue;
        }
        if (environment_sensor_crc8(buffer, 2U) != buffer[2] || environment_sensor_crc8(&buffer[3], 2U) != buffer[5]) {
            err = ERR_INTERNAL;
            continue;
        }

        memcpy(g_last_measurement_raw, buffer, sizeof(g_last_measurement_raw));
        temp_ticks = (uint16_t)(((uint16_t)buffer[0] << 8U) | buffer[1]);
        humidity_ticks = (uint16_t)(((uint16_t)buffer[3] << 8U) | buffer[4]);

        *out_temperature_centi_c = (int16_t)(-4500 + (((int32_t)17500 * (int32_t)temp_ticks) / 65535));
        humidity_centi = -600 + (((int32_t)12500 * (int32_t)humidity_ticks) / 65535);
        if (humidity_centi < 0) {
            humidity_centi = 0;
        }
        if (humidity_centi > 10000) {
            humidity_centi = 10000;
        }
        *out_humidity_centi_pct = (uint16_t)humidity_centi;

        g_last_measurement_error = ERR_OK;
        g_last_reading_valid = true;
        g_last_temperature_centi_c = *out_temperature_centi_c;
        g_last_humidity_centi_pct = *out_humidity_centi_pct;
        return ERR_OK;
    }
    memset(g_last_measurement_raw, 0, sizeof(g_last_measurement_raw));
    g_last_measurement_error = err;
    return err;
}
#endif

error_code_t environment_sensor_init(void) {
    if (board_get_pin_map(&g_pin_map) != ERR_OK) {
        return ERR_INTERNAL;
    }
    if (board_get_i2c_device_map(&g_device_map) != ERR_OK) {
        return ERR_INTERNAL;
    }

    g_i2c_ready = false;
    g_sensor_present = false;
    g_serial_number_valid = false;
    g_serial_number = 0U;
    g_sensor_i2c_address = 0U;
    g_last_serial_error = ERR_NOT_FOUND;
    g_last_measurement_error = ERR_NOT_FOUND;
    memset(g_last_serial_raw, 0, sizeof(g_last_serial_raw));
    memset(g_last_measurement_raw, 0, sizeof(g_last_measurement_raw));
    g_last_reading_valid = false;
    g_last_temperature_centi_c = 0;
    g_last_humidity_centi_pct = 0U;

#ifdef ESP_PLATFORM
    if (environment_sensor_bind_device() == ERR_OK) {
        (void)environment_sensor_write_command(SHT4X_CMD_SOFT_RESET);
        vTaskDelay(pdMS_TO_TICKS(2));
        (void)environment_sensor_read_serial_number();
    }
#endif
    return ERR_OK;
}

error_code_t environment_sensor_get_status(platform_environment_status_t *out_status) {
#ifdef ESP_PLATFORM
    int16_t measured_temperature_centi_c = 0;
    uint16_t measured_humidity_centi_pct = 0;
    bool reading_valid = false;
#endif

    if (!out_status) {
        return ERR_INVALID_ARGS;
    }

    memset(out_status, 0, sizeof(*out_status));

#ifdef ESP_PLATFORM
    if (!g_sensor_present) {
        (void)environment_sensor_bind_device();
    }
    if (!g_serial_number_valid && g_last_serial_error == ERR_NOT_FOUND) {
        (void)environment_sensor_read_serial_number();
    }

    if (!g_sensor_present) {
        out_status->sensor_present = false;
        out_status->serial_number_valid = g_serial_number_valid;
        out_status->last_serial_error = g_last_serial_error;
        out_status->last_measurement_error = g_last_measurement_error;
        out_status->i2c_bus_index = ENVIRONMENT_SENSOR_I2C_BUS_INDEX;
        out_status->i2c_address = g_sensor_i2c_address;
        memcpy(out_status->serial_raw, g_last_serial_raw, sizeof(out_status->serial_raw));
        memcpy(out_status->measurement_raw, g_last_measurement_raw, sizeof(out_status->measurement_raw));
        out_status->serial_number = g_serial_number;
        return ERR_OK;
    }

    if (environment_sensor_measure(&measured_temperature_centi_c, &measured_humidity_centi_pct) == ERR_OK) {
        reading_valid = true;
    } else if (g_last_reading_valid) {
        measured_temperature_centi_c = g_last_temperature_centi_c;
        measured_humidity_centi_pct = g_last_humidity_centi_pct;
        reading_valid = true;
    }

    out_status->sensor_present = g_sensor_present;
    out_status->serial_number_valid = g_serial_number_valid;
    out_status->reading_valid = reading_valid;
    out_status->last_serial_error = g_last_serial_error;
    out_status->last_measurement_error = g_last_measurement_error;
    out_status->i2c_bus_index = ENVIRONMENT_SENSOR_I2C_BUS_INDEX;
    out_status->i2c_address = g_sensor_i2c_address;
    memcpy(out_status->serial_raw, g_last_serial_raw, sizeof(out_status->serial_raw));
    memcpy(out_status->measurement_raw, g_last_measurement_raw, sizeof(out_status->measurement_raw));
    out_status->serial_number = g_serial_number;
    out_status->temperature_centi_c = measured_temperature_centi_c;
    out_status->humidity_centi_pct = measured_humidity_centi_pct;
    return ERR_OK;
#else
    return ERR_OK;
#endif
}
