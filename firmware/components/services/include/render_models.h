#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "core/enums.h"
#include "core/types.h"

typedef struct {
    char text[2048];
    uint16_t x;
    uint16_t y;
    char font[64];
    uint16_t size;
    char align[16];
    bool commit;
    display_refresh_mode_t refresh_mode;
} render_text_req_t;

typedef struct {
    char asset_path[256];
    uint16_t x;
    uint16_t y;
    bool commit;
    display_refresh_mode_t refresh_mode;
} render_bitmap_req_t;

typedef struct {
    bool framebuffer_updated;
    bool display_refreshed;
    display_refresh_mode_t refresh_mode;
    rect_u16_t affected_region;
    uint32_t elapsed_ms;
} render_result_t;

typedef struct {
    uint32_t job_kind;
    bool framebuffer_updated;
    bool display_refreshed;
    display_refresh_mode_t refresh_mode;
    rect_u16_t affected_region;
    uint32_t elapsed_ms;
    uint32_t timestamp_ms;
    error_code_t error_code;
} last_render_record_t;

typedef struct {
    char reason[128];
    uint32_t wake_after_sec;
    bool wake_after_set;
} sleep_req_t;
