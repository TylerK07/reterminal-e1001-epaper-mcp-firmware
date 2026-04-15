#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "errors.h"
#include "types.h"

typedef struct {
    uint16_t width;
    uint16_t height;
    bool partial_refresh_supported;
} display_caps_t;

typedef enum {
    DISPLAY_BACKGROUND_TRANSPARENT = 0,
    DISPLAY_BACKGROUND_WHITE,
    DISPLAY_BACKGROUND_BLACK
} display_background_mode_t;

typedef enum {
    DISPLAY_FOREGROUND_BLACK = 0,
    DISPLAY_FOREGROUND_WHITE
} display_foreground_color_t;

typedef struct {
    char text[2048];
    uint16_t x;
    uint16_t y;
    char font[64];
    uint16_t size;
    char align[16];
    display_foreground_color_t foreground_color;
    display_background_mode_t background_mode;
} display_text_draw_req_t;

typedef struct {
    char asset_path[256];
    uint16_t x;
    uint16_t y;
} display_bitmap_draw_req_t;

typedef struct {
    char layout_name[64];
    char payload_json[1024];
} display_layout_draw_req_t;

error_code_t display_init(void);
error_code_t display_get_caps(display_caps_t *out_caps);
error_code_t display_clear_framebuffer(bool white);
error_code_t display_fill_region(const rect_u16_t *region, bool black);
error_code_t display_draw_text(const display_text_draw_req_t *req, rect_u16_t *out_region);
error_code_t display_draw_bitmap(const display_bitmap_draw_req_t *req, rect_u16_t *out_region);
error_code_t display_draw_layout(const display_layout_draw_req_t *req, rect_u16_t *out_region);
error_code_t display_draw_test_pattern(rect_u16_t *out_region);
error_code_t display_refresh_full(uint32_t *out_elapsed_ms);
error_code_t display_refresh_partial(const rect_u16_t *region, uint32_t *out_elapsed_ms);
bool display_is_busy(void);
