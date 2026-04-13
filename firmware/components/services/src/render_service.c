#include "services/render_service.h"
#include <string.h>

error_code_t render_service_init(void) { return ERR_OK; }

error_code_t render_service_render_text(const render_text_req_t *req, render_result_t *out_result) {
    if (!req || !out_result) return ERR_INVALID_ARGS;
    memset(out_result, 0, sizeof(*out_result));
    // TODO: route through display + policy checks
    out_result->framebuffer_updated = true;
    out_result->display_refreshed = req->commit;
    out_result->refresh_mode = req->refresh_mode;
    return ERR_OK;
}

error_code_t render_service_refresh(display_refresh_mode_t mode, const rect_u16_t *region, render_result_t *out_result) {
    if (!out_result) return ERR_INVALID_ARGS;
    memset(out_result, 0, sizeof(*out_result));
    out_result->display_refreshed = true;
    out_result->refresh_mode = mode;
    if (region) out_result->affected_region = *region;
    return ERR_OK;
}
