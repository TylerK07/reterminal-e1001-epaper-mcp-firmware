#include "platform/display_epaper.h"

error_code_t display_init(void) { return ERR_OK; }
error_code_t display_get_caps(display_caps_t *out_caps) {
    if (!out_caps) return ERR_INVALID_ARGS;
    out_caps->width = 800;
    out_caps->height = 480;
    out_caps->partial_refresh_supported = true;
    return ERR_OK;
}
error_code_t display_clear_framebuffer(bool white) { (void)white; return ERR_OK; }
error_code_t display_draw_text(const render_text_req_t *req, rect_u16_t *out_region) {
    if (!req || !out_region) return ERR_INVALID_ARGS;
    out_region->x = req->x; out_region->y = req->y; out_region->w = 100; out_region->h = 20;
    return ERR_OK;
}
error_code_t display_refresh_full(uint32_t *out_elapsed_ms) { if (out_elapsed_ms) *out_elapsed_ms = 1000; return ERR_OK; }
error_code_t display_refresh_partial(const rect_u16_t *region, uint32_t *out_elapsed_ms) { (void)region; if (out_elapsed_ms) *out_elapsed_ms = 250; return ERR_OK; }
bool display_is_busy(void) { return false; }
