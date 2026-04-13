#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "core/errors.h"
#include "core/types.h"
#include "services/render_models.h"

typedef struct {
    uint16_t width;
    uint16_t height;
    bool partial_refresh_supported;
} display_caps_t;

error_code_t display_init(void);
error_code_t display_get_caps(display_caps_t *out_caps);
error_code_t display_clear_framebuffer(bool white);
error_code_t display_draw_text(const render_text_req_t *req, rect_u16_t *out_region);
error_code_t display_refresh_full(uint32_t *out_elapsed_ms);
error_code_t display_refresh_partial(const rect_u16_t *region, uint32_t *out_elapsed_ms);
bool display_is_busy(void);
