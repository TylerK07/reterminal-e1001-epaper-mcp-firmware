#include "display_epaper.h"
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "board.h"
#ifdef ESP_PLATFORM
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif

static board_pin_map_t g_pin_map;
static board_display_profile_t g_display_profile;
static uint8_t g_framebuffer[(800U * 480U) / 8U];
static uint8_t g_committed_framebuffer[(800U * 480U) / 8U];
static uint8_t g_partial_row_buffer[800U / 8U];
static bool g_initialized = false;
static bool g_busy = false;
static uint32_t g_last_refresh_ms = 0U;
static display_refresh_mode_t g_last_refresh_mode = DISPLAY_REFRESH_NONE;
static bool g_partial_mode_active = false;
static bool g_panel_powered = false;
static uint8_t g_partial_refresh_count = 0U;

typedef enum {
    UC8179_DRIVE_MODE_FULL = 0,
    UC8179_DRIVE_MODE_FAST,
    UC8179_DRIVE_MODE_PARTIAL
} uc8179_drive_mode_t;

static uc8179_drive_mode_t g_drive_mode = UC8179_DRIVE_MODE_FULL;
#ifdef ESP_PLATFORM
static spi_device_handle_t g_display_spi = NULL;
static bool g_spi_bus_ready = false;
static const size_t g_spi_chunk_size = 4092U;
#endif

/*
 * The E1001 panel matches Good Display GDEY075T7 and Seeed's Arduino docs call
 * out UC8179 explicitly. The command IDs below follow the UC8179 family command
 * map and still need byte-for-byte validation against vendor sample code before
 * we enable real panel traffic.
 */
typedef enum {
    UC8179_CMD_PANEL_SETTING = 0x00,
    UC8179_CMD_POWER_SETTING = 0x01,
    UC8179_CMD_POWER_OFF = 0x02,
    UC8179_CMD_POWER_ON = 0x04,
    UC8179_CMD_BOOSTER_SOFT_START = 0x06,
    UC8179_CMD_DEEP_SLEEP = 0x07,
    UC8179_CMD_DATA_START_TRANSMISSION_1 = 0x10,
    UC8179_CMD_DISPLAY_REFRESH = 0x12,
    UC8179_CMD_DATA_START_TRANSMISSION_2 = 0x13,
    UC8179_CMD_DUAL_SPI_MODE = 0x15,
    UC8179_CMD_AUTO_SEQUENCE = 0x17,
    UC8179_CMD_VCOM_AND_DATA_INTERVAL_SETTING = 0x50,
    UC8179_CMD_TCON_SETTING = 0x60,
    UC8179_CMD_TCON_RESOLUTION = 0x61,
    UC8179_CMD_PARTIAL_WINDOW = 0x90,
    UC8179_CMD_PARTIAL_IN = 0x91,
    UC8179_CMD_PARTIAL_OUT = 0x92,
    UC8179_CMD_PROGRAM_MODE = 0xE0,
    UC8179_CMD_POWER_SAVING = 0xE5
} uc8179_command_t;

typedef struct {
    uc8179_command_t command;
    uint8_t data[8];
    uint8_t data_len;
    bool requires_busy_wait;
} uc8179_sequence_step_t;

static error_code_t uc8179_write_data(const uint8_t *data, size_t data_len);
static error_code_t uc8179_begin_framebuffer_write(bool previous_frame_plane);
static error_code_t uc8179_write_command(uc8179_command_t command, const uint8_t *data, size_t data_len);
static error_code_t uc8179_wait_while_busy(void);
static error_code_t uc8179_power_on_if_needed(void);
static error_code_t uc8179_power_off_if_needed(void);
static error_code_t uc8179_enter_partial_window(const rect_u16_t *region);
static error_code_t uc8179_exit_partial_window(void);

static const uc8179_sequence_step_t g_uc8179_init_sequence[] = {
    {UC8179_CMD_POWER_SETTING, {0x07, 0x07, 0x3F, 0x3F}, 4U, false},
    {UC8179_CMD_BOOSTER_SOFT_START, {0x17, 0x17, 0x28, 0x17}, 4U, false},
    {UC8179_CMD_POWER_ON, {0x00}, 0U, true},
    {UC8179_CMD_PANEL_SETTING, {0x1F}, 1U, false},
    {UC8179_CMD_TCON_RESOLUTION, {0x03, 0x20, 0x01, 0xE0}, 4U, false},
    {UC8179_CMD_DUAL_SPI_MODE, {0x00}, 1U, false},
    {UC8179_CMD_VCOM_AND_DATA_INTERVAL_SETTING, {0x10, 0x07}, 2U, false},
    {UC8179_CMD_TCON_SETTING, {0x22}, 1U, false},
};

static const uc8179_sequence_step_t g_uc8179_fast_init_sequence[] = {
    {UC8179_CMD_PANEL_SETTING, {0x1F}, 1U, false},
    {UC8179_CMD_VCOM_AND_DATA_INTERVAL_SETTING, {0x10, 0x07}, 2U, false},
    {UC8179_CMD_POWER_ON, {0x00}, 0U, true},
    {UC8179_CMD_BOOSTER_SOFT_START, {0x27, 0x27, 0x18, 0x17}, 4U, false},
    {UC8179_CMD_PROGRAM_MODE, {0x02}, 1U, false},
    {UC8179_CMD_POWER_SAVING, {0x5A}, 1U, false},
};

static const uc8179_sequence_step_t g_uc8179_partial_init_sequence[] = {
    {UC8179_CMD_PANEL_SETTING, {0x1F}, 1U, false},
    {UC8179_CMD_POWER_ON, {0x00}, 0U, true},
    {UC8179_CMD_PROGRAM_MODE, {0x02}, 1U, false},
    {UC8179_CMD_POWER_SAVING, {0x6E}, 1U, false},
};

static uint16_t display_get_bytes_per_row(void) {
    return (uint16_t)(g_display_profile.width / 8U);
}

static void display_framebuffer_fill(bool white) {
    memset(g_framebuffer, white ? 0x00 : 0xFF, sizeof(g_framebuffer));
}

static void display_copy_framebuffer_state(void) {
    memcpy(g_committed_framebuffer, g_framebuffer, sizeof(g_framebuffer));
}

static void display_copy_region_to_committed(const rect_u16_t *region) {
    uint16_t bytes_per_row;
    uint16_t start_byte;
    uint16_t row_byte_count;
    uint16_t row;

    if (!region) {
        return;
    }

    bytes_per_row = display_get_bytes_per_row();
    start_byte = (uint16_t)(region->x / 8U);
    row_byte_count = (uint16_t)(region->w / 8U);

    for (row = 0U; row < region->h; ++row) {
        uint32_t offset = ((uint32_t)(region->y + row) * (uint32_t)bytes_per_row) + (uint32_t)start_byte;
        memcpy(&g_committed_framebuffer[offset], &g_framebuffer[offset], row_byte_count);
    }
}

static error_code_t display_normalize_partial_region(const rect_u16_t *region, rect_u16_t *out_region) {
    uint16_t x0;
    uint16_t x1;
    uint16_t y1;

    if (!region || !out_region) {
        return ERR_INVALID_ARGS;
    }
    if (region->w == 0U || region->h == 0U) {
        return ERR_INVALID_ARGS;
    }
    if (region->x >= g_display_profile.width || region->y >= g_display_profile.height) {
        return ERR_INVALID_ARGS;
    }

    x0 = (uint16_t)(region->x & 0xFFF8U);
    x1 = (uint16_t)(region->x + region->w);
    if (x1 > g_display_profile.width) {
        x1 = g_display_profile.width;
    }
    x1 = (uint16_t)((x1 + 7U) & 0xFFF8U);
    if (x1 > g_display_profile.width) {
        x1 = g_display_profile.width;
    }

    y1 = (uint16_t)(region->y + region->h);
    if (y1 > g_display_profile.height) {
        y1 = g_display_profile.height;
    }

    out_region->x = x0;
    out_region->y = region->y;
    out_region->w = (uint16_t)(x1 - x0);
    out_region->h = (uint16_t)(y1 - region->y);
    if (out_region->w == 0U || out_region->h == 0U) {
        return ERR_INVALID_ARGS;
    }
    return ERR_OK;
}

static void display_set_pixel(uint16_t x, uint16_t y, bool black) {
    uint32_t byte_index;
    uint8_t mask;

    if (x >= g_display_profile.width || y >= g_display_profile.height) {
        return;
    }

    byte_index = ((uint32_t)y * (uint32_t)display_get_bytes_per_row()) + ((uint32_t)x / 8U);
    mask = (uint8_t)(0x80U >> (x & 0x07U));
    if (black) {
        g_framebuffer[byte_index] |= mask;
    } else {
        g_framebuffer[byte_index] &= (uint8_t)(~mask);
    }
}

error_code_t display_fill_region(const rect_u16_t *region, bool black) {
    uint16_t x_end;
    uint16_t y_end;
    uint16_t y;

    if (!g_initialized || !region) {
        return ERR_INVALID_ARGS;
    }
    if (region->w == 0U || region->h == 0U) {
        return ERR_INVALID_ARGS;
    }
    if (region->x >= g_display_profile.width || region->y >= g_display_profile.height) {
        return ERR_INVALID_ARGS;
    }

    x_end = (uint16_t)(region->x + region->w);
    y_end = (uint16_t)(region->y + region->h);
    if (x_end > g_display_profile.width) {
        x_end = g_display_profile.width;
    }
    if (y_end > g_display_profile.height) {
        y_end = g_display_profile.height;
    }

    for (y = region->y; y < y_end; ++y) {
        uint16_t x;
        for (x = region->x; x < x_end; ++x) {
            display_set_pixel(x, y, black);
        }
    }

    return ERR_OK;
}

static const uint8_t *display_get_glyph(char c) {
    static const uint8_t glyph_space[5] = {0x00, 0x00, 0x00, 0x00, 0x00};
    static const uint8_t glyph_question[5] = {0x02, 0x01, 0x51, 0x09, 0x06};
    static const uint8_t glyph_dash[5] = {0x08, 0x08, 0x08, 0x08, 0x08};
    static const uint8_t glyph_dot[5] = {0x00, 0x60, 0x60, 0x00, 0x00};
    static const uint8_t glyph_colon[5] = {0x00, 0x36, 0x36, 0x00, 0x00};
    static const uint8_t glyph_slash[5] = {0x20, 0x10, 0x08, 0x04, 0x02};
    static const uint8_t glyph_percent[5] = {0x62, 0x64, 0x08, 0x13, 0x23};
    static const uint8_t glyph_underscore[5] = {0x40, 0x40, 0x40, 0x40, 0x40};
    static const uint8_t digits[10][5] = {
        {0x3E, 0x51, 0x49, 0x45, 0x3E},
        {0x00, 0x42, 0x7F, 0x40, 0x00},
        {0x42, 0x61, 0x51, 0x49, 0x46},
        {0x21, 0x41, 0x45, 0x4B, 0x31},
        {0x18, 0x14, 0x12, 0x7F, 0x10},
        {0x27, 0x45, 0x45, 0x45, 0x39},
        {0x3C, 0x4A, 0x49, 0x49, 0x30},
        {0x01, 0x71, 0x09, 0x05, 0x03},
        {0x36, 0x49, 0x49, 0x49, 0x36},
        {0x06, 0x49, 0x49, 0x29, 0x1E}
    };
    static const uint8_t letters[26][5] = {
        {0x7E, 0x11, 0x11, 0x11, 0x7E},
        {0x7F, 0x49, 0x49, 0x49, 0x36},
        {0x3E, 0x41, 0x41, 0x41, 0x22},
        {0x7F, 0x41, 0x41, 0x22, 0x1C},
        {0x7F, 0x49, 0x49, 0x49, 0x41},
        {0x7F, 0x09, 0x09, 0x09, 0x01},
        {0x3E, 0x41, 0x49, 0x49, 0x7A},
        {0x7F, 0x08, 0x08, 0x08, 0x7F},
        {0x00, 0x41, 0x7F, 0x41, 0x00},
        {0x20, 0x40, 0x41, 0x3F, 0x01},
        {0x7F, 0x08, 0x14, 0x22, 0x41},
        {0x7F, 0x40, 0x40, 0x40, 0x40},
        {0x7F, 0x02, 0x0C, 0x02, 0x7F},
        {0x7F, 0x04, 0x08, 0x10, 0x7F},
        {0x3E, 0x41, 0x41, 0x41, 0x3E},
        {0x7F, 0x09, 0x09, 0x09, 0x06},
        {0x3E, 0x41, 0x51, 0x21, 0x5E},
        {0x7F, 0x09, 0x19, 0x29, 0x46},
        {0x46, 0x49, 0x49, 0x49, 0x31},
        {0x01, 0x01, 0x7F, 0x01, 0x01},
        {0x3F, 0x40, 0x40, 0x40, 0x3F},
        {0x1F, 0x20, 0x40, 0x20, 0x1F},
        {0x3F, 0x40, 0x38, 0x40, 0x3F},
        {0x63, 0x14, 0x08, 0x14, 0x63},
        {0x03, 0x04, 0x78, 0x04, 0x03},
        {0x61, 0x51, 0x49, 0x45, 0x43}
    };

    if (c >= 'a' && c <= 'z') {
        c = (char)(c - ('a' - 'A'));
    }

    if (c >= '0' && c <= '9') {
        return digits[(int)(c - '0')];
    }
    if (c >= 'A' && c <= 'Z') {
        return letters[(int)(c - 'A')];
    }

    switch (c) {
        case ' ': return glyph_space;
        case '-': return glyph_dash;
        case '.': return glyph_dot;
        case ':': return glyph_colon;
        case '/': return glyph_slash;
        case '%': return glyph_percent;
        case '_': return glyph_underscore;
        default: return glyph_question;
    }
}

static uint16_t display_font_scale_for_size(uint16_t size) {
    if (size >= 32U) {
        return 4U;
    }
    if (size >= 24U) {
        return 3U;
    }
    if (size >= 16U) {
        return 2U;
    }
    return 1U;
}

static void display_draw_glyph(char c, uint16_t x, uint16_t y, uint16_t scale, bool draw_black) {
    uint16_t col;

    for (col = 0U; col < 5U; ++col) {
        uint8_t column_bits = display_get_glyph(c)[col];
        uint16_t row;
        for (row = 0U; row < 7U; ++row) {
            uint16_t dx;
            uint16_t dy;
            bool glyph_pixel_on = ((column_bits >> row) & 0x01U) != 0U;
            for (dy = 0U; dy < scale; ++dy) {
                for (dx = 0U; dx < scale; ++dx) {
                    if (glyph_pixel_on) {
                        display_set_pixel(
                            (uint16_t)(x + (col * scale) + dx),
                            (uint16_t)(y + (row * scale) + dy),
                            draw_black);
                    }
                }
            }
        }
    }
}

static error_code_t display_upload_framebuffer(bool upload_previous_plane, bool upload_current_plane) {
    if (!g_initialized) {
        return ERR_INTERNAL;
    }
    if (uc8179_power_on_if_needed() != ERR_OK) {
        return ERR_INTERNAL;
    }

    if (upload_previous_plane) {
        if (uc8179_begin_framebuffer_write(true) != ERR_OK) {
            return ERR_INTERNAL;
        }
        if (uc8179_write_data(g_committed_framebuffer, sizeof(g_committed_framebuffer)) != ERR_OK) {
            return ERR_INTERNAL;
        }
    }

    if (upload_current_plane) {
        if (uc8179_begin_framebuffer_write(false) != ERR_OK) {
            return ERR_INTERNAL;
        }
        if (uc8179_write_data(g_framebuffer, sizeof(g_framebuffer)) != ERR_OK) {
            return ERR_INTERNAL;
        }
    }

    return ERR_OK;
}

static error_code_t display_upload_partial_region_data(const rect_u16_t *region) {
    uint16_t bytes_per_row;
    uint16_t start_byte;
    uint16_t row_byte_count;
    uint16_t row;

    if (!g_initialized || !region) {
        return ERR_INVALID_ARGS;
    }

    bytes_per_row = display_get_bytes_per_row();
    start_byte = (uint16_t)(region->x / 8U);
    row_byte_count = (uint16_t)(region->w / 8U);
    if (row_byte_count > sizeof(g_partial_row_buffer)) {
        return ERR_INVALID_ARGS;
    }
    if (uc8179_power_on_if_needed() != ERR_OK) {
        return ERR_INTERNAL;
    }

    if (uc8179_begin_framebuffer_write(true) != ERR_OK) {
        return ERR_INTERNAL;
    }
    for (row = 0U; row < region->h; ++row) {
        uint16_t i;
        uint32_t offset = ((uint32_t)(region->y + row) * (uint32_t)bytes_per_row) + (uint32_t)start_byte;
        memcpy(g_partial_row_buffer, &g_committed_framebuffer[offset], row_byte_count);
        for (i = 0U; i < row_byte_count; ++i) {
            g_partial_row_buffer[i] = (uint8_t)(~g_partial_row_buffer[i]);
        }
        if (uc8179_write_data(g_partial_row_buffer, row_byte_count) != ERR_OK) {
            return ERR_INTERNAL;
        }
    }

    if (uc8179_begin_framebuffer_write(false) != ERR_OK) {
        return ERR_INTERNAL;
    }

    for (row = 0U; row < region->h; ++row) {
        uint16_t i;
        uint32_t offset = ((uint32_t)(region->y + row) * (uint32_t)bytes_per_row) + (uint32_t)start_byte;
        memcpy(g_partial_row_buffer, &g_framebuffer[offset], row_byte_count);
        for (i = 0U; i < row_byte_count; ++i) {
            g_partial_row_buffer[i] = (uint8_t)(~g_partial_row_buffer[i]);
        }
        if (uc8179_write_data(g_partial_row_buffer, row_byte_count) != ERR_OK) {
            return ERR_INTERNAL;
        }
    }

    return ERR_OK;
}

#ifdef ESP_PLATFORM
static uint32_t display_get_framebuffer_size(void) {
    return ((uint32_t)g_display_profile.width * (uint32_t)g_display_profile.height) / 8U;
}

static error_code_t display_gpio_init(void) {
    gpio_config_t output_config = {
        .pin_bit_mask =
            (1ULL << g_pin_map.screen_cs_gpio) |
            (1ULL << g_pin_map.screen_dc_gpio) |
            (1ULL << g_pin_map.screen_reset_gpio),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config_t input_config = {
        .pin_bit_mask = (1ULL << g_pin_map.screen_busy_gpio),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    if (gpio_config(&output_config) != ESP_OK) {
        return ERR_INTERNAL;
    }
    if (gpio_config(&input_config) != ESP_OK) {
        return ERR_INTERNAL;
    }

    gpio_set_level((gpio_num_t)g_pin_map.screen_cs_gpio, 1);
    gpio_set_level((gpio_num_t)g_pin_map.screen_dc_gpio, 1);
    gpio_set_level((gpio_num_t)g_pin_map.screen_reset_gpio, 1);
    return ERR_OK;
}

static error_code_t display_spi_init(void) {
    spi_bus_config_t bus_config = {
        .mosi_io_num = g_pin_map.screen_mosi_gpio,
        .miso_io_num = g_pin_map.screen_miso_gpio,
        .sclk_io_num = g_pin_map.screen_sck_gpio,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = (int)display_get_framebuffer_size(),
    };
    spi_device_interface_config_t device_config = {
        .clock_speed_hz = 10 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = -1,
        .queue_size = 1,
    };

    if (!g_spi_bus_ready) {
        if (spi_bus_initialize(SPI2_HOST, &bus_config, SPI_DMA_CH_AUTO) != ESP_OK) {
            return ERR_INTERNAL;
        }
        g_spi_bus_ready = true;
    }

    if (!g_display_spi) {
        if (spi_bus_add_device(SPI2_HOST, &device_config, &g_display_spi) != ESP_OK) {
            return ERR_INTERNAL;
        }
    }

    return ERR_OK;
}
#endif

static error_code_t uc8179_hw_reset(void) {
#ifdef ESP_PLATFORM
    gpio_set_level((gpio_num_t)g_pin_map.screen_reset_gpio, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level((gpio_num_t)g_pin_map.screen_reset_gpio, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
#endif
    return ERR_OK;
}

static error_code_t uc8179_wait_while_busy(void) {
#ifdef ESP_PLATFORM
    TickType_t start_ticks = xTaskGetTickCount();
    const TickType_t timeout_ticks = pdMS_TO_TICKS(15000);

    while (gpio_get_level((gpio_num_t)g_pin_map.screen_busy_gpio) == 0) {
        if ((xTaskGetTickCount() - start_ticks) > timeout_ticks) {
            g_busy = false;
            return ERR_TIMEOUT;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
#endif
    g_busy = false;
    return ERR_OK;
}

static error_code_t uc8179_write_command(uc8179_command_t command, const uint8_t *data, size_t data_len) {
#ifdef ESP_PLATFORM
    spi_transaction_t transaction = {0};
    uint8_t command_byte = (uint8_t)command;
    esp_err_t esp_err;

    if (!g_display_spi) {
        return ERR_INTERNAL;
    }

    gpio_set_level((gpio_num_t)g_pin_map.screen_cs_gpio, 0);
    gpio_set_level((gpio_num_t)g_pin_map.screen_dc_gpio, 0);
    transaction.length = 8;
    transaction.tx_buffer = &command_byte;
    esp_err = spi_device_polling_transmit(g_display_spi, &transaction);
    gpio_set_level((gpio_num_t)g_pin_map.screen_cs_gpio, 1);
    if (esp_err != ESP_OK) {
        return ERR_INTERNAL;
    }

    if (data && data_len > 0U) {
        memset(&transaction, 0, sizeof(transaction));
        gpio_set_level((gpio_num_t)g_pin_map.screen_cs_gpio, 0);
        gpio_set_level((gpio_num_t)g_pin_map.screen_dc_gpio, 1);
        transaction.length = data_len * 8U;
        transaction.tx_buffer = data;
        esp_err = spi_device_polling_transmit(g_display_spi, &transaction);
        gpio_set_level((gpio_num_t)g_pin_map.screen_cs_gpio, 1);
        if (esp_err != ESP_OK) {
            return ERR_INTERNAL;
        }
    }
#else
    (void)command;
    (void)data;
    (void)data_len;
#endif
    return ERR_OK;
}

static error_code_t uc8179_write_data(const uint8_t *data, size_t data_len) {
#ifdef ESP_PLATFORM
    spi_transaction_t transaction = {0};
    size_t offset = 0U;
    if (!data || data_len == 0U) {
        return ERR_INVALID_ARGS;
    }
    if (!g_display_spi) {
        return ERR_INTERNAL;
    }

    gpio_set_level((gpio_num_t)g_pin_map.screen_cs_gpio, 0);
    gpio_set_level((gpio_num_t)g_pin_map.screen_dc_gpio, 1);
    while (offset < data_len) {
        size_t chunk_len = data_len - offset;
        if (chunk_len > g_spi_chunk_size) {
            chunk_len = g_spi_chunk_size;
        }

        memset(&transaction, 0, sizeof(transaction));
        transaction.length = chunk_len * 8U;
        transaction.tx_buffer = data + offset;
        if (spi_device_polling_transmit(g_display_spi, &transaction) != ESP_OK) {
            gpio_set_level((gpio_num_t)g_pin_map.screen_cs_gpio, 1);
            return ERR_INTERNAL;
        }
        offset += chunk_len;
    }
    gpio_set_level((gpio_num_t)g_pin_map.screen_cs_gpio, 1);
#else
    (void)data;
    (void)data_len;
#endif
    return ERR_OK;
}

static error_code_t uc8179_run_sequence(const uc8179_sequence_step_t *steps, size_t step_count) {
    size_t i;
    error_code_t err;

    if (!steps) {
        return ERR_INVALID_ARGS;
    }

    for (i = 0; i < step_count; ++i) {
        err = uc8179_write_command(steps[i].command, steps[i].data, steps[i].data_len);
        if (err != ERR_OK) {
            return err;
        }

        if (steps[i].requires_busy_wait) {
            err = uc8179_wait_while_busy();
            if (err != ERR_OK) {
                return err;
            }
        }
    }

    return ERR_OK;
}

static error_code_t uc8179_initialize_panel(void) {
    error_code_t err = uc8179_hw_reset();
    if (err != ERR_OK) {
        return err;
    }

    err = uc8179_run_sequence(
        g_uc8179_init_sequence,
        sizeof(g_uc8179_init_sequence) / sizeof(g_uc8179_init_sequence[0]));
    if (err != ERR_OK) {
        return err;
    }

    g_initialized = true;
    g_drive_mode = UC8179_DRIVE_MODE_FULL;
    g_partial_mode_active = false;
    g_panel_powered = true;
    g_partial_refresh_count = 0U;
    return ERR_OK;
}

static error_code_t uc8179_initialize_mode(uc8179_drive_mode_t mode) {
    const uc8179_sequence_step_t *sequence = NULL;
    size_t sequence_len = 0U;
    error_code_t err;

    err = uc8179_hw_reset();
    if (err != ERR_OK) {
        return err;
    }

    switch (mode) {
        case UC8179_DRIVE_MODE_FULL:
            sequence = g_uc8179_init_sequence;
            sequence_len = sizeof(g_uc8179_init_sequence) / sizeof(g_uc8179_init_sequence[0]);
            break;
        case UC8179_DRIVE_MODE_FAST:
            sequence = g_uc8179_fast_init_sequence;
            sequence_len = sizeof(g_uc8179_fast_init_sequence) / sizeof(g_uc8179_fast_init_sequence[0]);
            break;
        case UC8179_DRIVE_MODE_PARTIAL:
            sequence = g_uc8179_partial_init_sequence;
            sequence_len = sizeof(g_uc8179_partial_init_sequence) / sizeof(g_uc8179_partial_init_sequence[0]);
            break;
        default:
            return ERR_INVALID_ARGS;
    }

    err = uc8179_run_sequence(sequence, sequence_len);
    if (err != ERR_OK) {
        return err;
    }

    g_initialized = true;
    g_drive_mode = mode;
    g_partial_mode_active = false;
    g_panel_powered = true;
    return ERR_OK;
}

static error_code_t uc8179_power_on_if_needed(void) {
    if (g_panel_powered) {
        return ERR_OK;
    }
    if (uc8179_write_command(UC8179_CMD_POWER_ON, NULL, 0U) != ERR_OK) {
        return ERR_INTERNAL;
    }
    g_busy = true;
    if (uc8179_wait_while_busy() != ERR_OK) {
        return ERR_INTERNAL;
    }
    g_panel_powered = true;
    return ERR_OK;
}

static error_code_t uc8179_power_off_if_needed(void) {
    if (!g_panel_powered) {
        return ERR_OK;
    }
    if (uc8179_write_command(UC8179_CMD_POWER_OFF, NULL, 0U) != ERR_OK) {
        return ERR_INTERNAL;
    }
    g_busy = true;
    if (uc8179_wait_while_busy() != ERR_OK) {
        return ERR_INTERNAL;
    }
    g_panel_powered = false;
    return ERR_OK;
}

static uint8_t display_partial_budget(void) {
    return 6U;
}

static error_code_t uc8179_begin_framebuffer_write(bool previous_frame_plane) {
    return uc8179_write_command(
        previous_frame_plane ? UC8179_CMD_DATA_START_TRANSMISSION_1 : UC8179_CMD_DATA_START_TRANSMISSION_2,
        NULL,
        0U);
}

static error_code_t uc8179_enter_partial_window(const rect_u16_t *region) {
    uint8_t partial_window[9];
    const uint8_t partial_vcom_data[] = {0xA9, 0x07};
    uint16_t x_end;
    uint16_t y_end;

    if (!region) {
        return ERR_INVALID_ARGS;
    }

    x_end = (uint16_t)(region->x + region->w - 1U);
    y_end = (uint16_t)(region->y + region->h - 1U);

    if (uc8179_write_command(
            UC8179_CMD_VCOM_AND_DATA_INTERVAL_SETTING,
            partial_vcom_data,
            sizeof(partial_vcom_data)) != ERR_OK) {
        return ERR_INTERNAL;
    }

    partial_window[0] = (uint8_t)(region->x >> 8);
    partial_window[1] = (uint8_t)(region->x & 0xFFU);
    partial_window[2] = (uint8_t)(x_end >> 8);
    partial_window[3] = (uint8_t)(x_end & 0xFFU);
    partial_window[4] = (uint8_t)(region->y >> 8);
    partial_window[5] = (uint8_t)(region->y & 0xFFU);
    partial_window[6] = (uint8_t)(y_end >> 8);
    partial_window[7] = (uint8_t)(y_end & 0xFFU);
    partial_window[8] = 0x01U;

    if (uc8179_write_command(UC8179_CMD_PARTIAL_WINDOW, partial_window, sizeof(partial_window)) != ERR_OK) {
        return ERR_INTERNAL;
    }
    if (uc8179_write_command(UC8179_CMD_PARTIAL_IN, NULL, 0U) != ERR_OK) {
        return ERR_INTERNAL;
    }

    g_partial_mode_active = true;
    return ERR_OK;
}

static error_code_t uc8179_exit_partial_window(void) {
    if (uc8179_write_command(UC8179_CMD_PARTIAL_OUT, NULL, 0U) != ERR_OK) {
        return ERR_INTERNAL;
    }
    g_partial_mode_active = false;
    return ERR_OK;
}

static error_code_t uc8179_trigger_refresh(display_refresh_mode_t mode, const rect_u16_t *region) {
    if (!g_initialized) {
        return ERR_INTERNAL;
    }

    if (mode == DISPLAY_REFRESH_PARTIAL && region) {
        if (uc8179_enter_partial_window(region) != ERR_OK) {
            return ERR_INTERNAL;
        }
    }

    g_busy = true;
    if (uc8179_write_command(UC8179_CMD_DISPLAY_REFRESH, NULL, 0U) != ERR_OK) {
        g_busy = false;
        return ERR_INTERNAL;
    }
    if (uc8179_wait_while_busy() != ERR_OK) {
        return ERR_INTERNAL;
    }

    if (mode == DISPLAY_REFRESH_PARTIAL && region) {
        if (uc8179_exit_partial_window() != ERR_OK) {
            return ERR_INTERNAL;
        }
    }

    g_last_refresh_mode = mode;
    g_last_refresh_ms = (mode == DISPLAY_REFRESH_PARTIAL)
        ? g_display_profile.partial_refresh_time_ms
        : g_display_profile.full_refresh_time_ms;
    if (uc8179_power_off_if_needed() != ERR_OK) {
        return ERR_INTERNAL;
    }
    return ERR_OK;
}

error_code_t display_init(void) {
    if (board_get_pin_map(&g_pin_map) != ERR_OK) {
        return ERR_INTERNAL;
    }
    if (board_get_display_profile(&g_display_profile) != ERR_OK) {
        return ERR_INTERNAL;
    }
#ifdef ESP_PLATFORM
    if (display_gpio_init() != ERR_OK) {
        return ERR_INTERNAL;
    }
    if (display_spi_init() != ERR_OK) {
        return ERR_INTERNAL;
    }
#endif

    display_framebuffer_fill(true);
    memset(g_committed_framebuffer, 0x00, sizeof(g_committed_framebuffer));
    return uc8179_initialize_panel();
}
error_code_t display_get_caps(display_caps_t *out_caps) {
    if (!out_caps) return ERR_INVALID_ARGS;
    out_caps->width = g_display_profile.width;
    out_caps->height = g_display_profile.height;
    out_caps->partial_refresh_supported = g_display_profile.supports_partial_refresh;
    return ERR_OK;
}
error_code_t display_clear_framebuffer(bool white) {
    if (!g_initialized) {
        return ERR_INTERNAL;
    }
    display_framebuffer_fill(white);
    return ERR_OK;
}
error_code_t display_draw_text(const display_text_draw_req_t *req, rect_u16_t *out_region) {
    size_t i;
    uint16_t cursor_x;
    uint16_t cursor_y;
    uint16_t scale;
    uint16_t glyph_w;
    uint16_t glyph_h;
    uint16_t max_line_width = 0U;
    uint16_t line_width = 0U;
    uint16_t line_count = 1U;
    rect_u16_t background_region;

    if (!req || !out_region) return ERR_INVALID_ARGS;
    if (!g_initialized) {
        return ERR_INTERNAL;
    }

    scale = display_font_scale_for_size(req->size);
    glyph_w = (uint16_t)(6U * scale);
    glyph_h = (uint16_t)(8U * scale);
    cursor_x = req->x;
    cursor_y = req->y;

    out_region->x = req->x;
    out_region->y = req->y;
    out_region->w = glyph_w;
    out_region->h = glyph_h;

    for (i = 0U; req->text[i] != '\0'; ++i) {
        if (req->text[i] == '\n') {
            if (line_width > max_line_width) {
                max_line_width = line_width;
            }
            line_width = 0U;
            ++line_count;
            continue;
        }
        line_width = (uint16_t)(line_width + glyph_w);
    }
    if (line_width > max_line_width) {
        max_line_width = line_width;
    }
    if (max_line_width > 0U) {
        out_region->w = max_line_width;
    }
    out_region->h = (uint16_t)(glyph_h * line_count);

    if (req->background_mode != DISPLAY_BACKGROUND_TRANSPARENT) {
        background_region = *out_region;
        if (display_fill_region(&background_region, req->background_mode == DISPLAY_BACKGROUND_BLACK) != ERR_OK) {
            return ERR_INTERNAL;
        }
    }

    for (i = 0U; req->text[i] != '\0'; ++i) {
        char c = req->text[i];

        if (c == '\n') {
            cursor_x = req->x;
            cursor_y = (uint16_t)(cursor_y + glyph_h);
            continue;
        }

        display_draw_glyph(c, cursor_x, cursor_y, scale, req->foreground_color != DISPLAY_FOREGROUND_WHITE);
        cursor_x = (uint16_t)(cursor_x + glyph_w);
    }

    return ERR_OK;
}
error_code_t display_draw_bitmap(const display_bitmap_draw_req_t *req, rect_u16_t *out_region) {
    if (!req || !out_region) return ERR_INVALID_ARGS;
    if (!g_initialized) {
        return ERR_INTERNAL;
    }
    if (uc8179_begin_framebuffer_write(false) != ERR_OK) {
        return ERR_INTERNAL;
    }
    out_region->x = req->x; out_region->y = req->y; out_region->w = 120; out_region->h = 120;
    /* TODO: stream 1bpp/2bpp bitmap payloads in the panel's expected order. */
    return ERR_OK;
}
error_code_t display_draw_layout(const display_layout_draw_req_t *req, rect_u16_t *out_region) {
    if (!req || !out_region) return ERR_INVALID_ARGS;
    (void)req;
    if (!g_initialized) {
        return ERR_INTERNAL;
    }
    out_region->x = 0; out_region->y = 0; out_region->w = 800; out_region->h = 480;
    return ERR_OK;
}
error_code_t display_draw_test_pattern(rect_u16_t *out_region) {
    uint16_t row = 0U;
    const uint16_t bytes_per_row = display_get_bytes_per_row();
    uint16_t col = 0U;

    if (!g_initialized || !out_region) {
        return ERR_INVALID_ARGS;
    }

    display_framebuffer_fill(true);
    for (row = 0U; row < g_display_profile.height; ++row) {
        for (col = 0U; col < bytes_per_row; ++col) {
            uint16_t x_base = (uint16_t)(col * 8U);
            uint8_t pattern = 0x00U;
            uint8_t bit;

            if (row < (g_display_profile.height / 3U)) {
                pattern = (uint8_t)((col & 0x01U) ? 0x00U : 0xFFU);
            } else if (row < ((g_display_profile.height * 2U) / 3U)) {
                pattern = 0xAAU;
            } else {
                pattern = (uint8_t)((col & 0x01U) ? 0xF0U : 0x0FU);
            }

            for (bit = 0U; bit < 8U; ++bit) {
                bool black = ((pattern >> (7U - bit)) & 0x01U) != 0U;
                display_set_pixel((uint16_t)(x_base + bit), row, black);
            }
        }
    }

    out_region->x = 0U;
    out_region->y = 0U;
    out_region->w = g_display_profile.width;
    out_region->h = g_display_profile.height;
    return ERR_OK;
}
error_code_t display_refresh_full(uint32_t *out_elapsed_ms) {
    error_code_t err;
    if (g_drive_mode != UC8179_DRIVE_MODE_FULL) {
        err = uc8179_initialize_mode(UC8179_DRIVE_MODE_FULL);
        if (err != ERR_OK) {
            return err;
        }
    }
    err = display_upload_framebuffer(true, true);
    if (err != ERR_OK) {
        return err;
    }
    err = uc8179_trigger_refresh(DISPLAY_REFRESH_FULL, NULL);
    if (err != ERR_OK) {
        return err;
    }
    display_copy_framebuffer_state();
    g_partial_refresh_count = 0U;
    if (out_elapsed_ms) {
        *out_elapsed_ms = g_last_refresh_ms;
    }
    return ERR_OK;
}
error_code_t display_refresh_partial(const rect_u16_t *region, uint32_t *out_elapsed_ms) {
    error_code_t err;
    rect_u16_t normalized_region;

    if (!region) {
        return ERR_INVALID_ARGS;
    }
    if (g_partial_refresh_count >= display_partial_budget()) {
        return display_refresh_full(out_elapsed_ms);
    }
    err = display_normalize_partial_region(region, &normalized_region);
    if (err != ERR_OK) {
        return err;
    }
    if (g_drive_mode != UC8179_DRIVE_MODE_PARTIAL) {
        err = uc8179_initialize_mode(UC8179_DRIVE_MODE_PARTIAL);
        if (err != ERR_OK) {
            return err;
        }
    }
    err = uc8179_enter_partial_window(&normalized_region);
    if (err != ERR_OK) {
        return err;
    }
    err = display_upload_partial_region_data(&normalized_region);
    if (err != ERR_OK) {
        (void)uc8179_exit_partial_window();
        return err;
    }
    err = uc8179_trigger_refresh(DISPLAY_REFRESH_PARTIAL, NULL);
    if (err != ERR_OK) {
        (void)uc8179_exit_partial_window();
        return err;
    }
    err = uc8179_exit_partial_window();
    if (err != ERR_OK) {
        return err;
    }
    display_copy_region_to_committed(&normalized_region);
    ++g_partial_refresh_count;
    if (out_elapsed_ms) {
        *out_elapsed_ms = g_last_refresh_ms;
    }
    return ERR_OK;
}
bool display_is_busy(void) { return g_busy; }
