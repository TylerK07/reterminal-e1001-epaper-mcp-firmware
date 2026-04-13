#pragma once
#include "core/errors.h"
#include "services/render_models.h"

error_code_t render_service_init(void);
error_code_t render_service_render_text(const render_text_req_t *req, render_result_t *out_result);
error_code_t render_service_refresh(display_refresh_mode_t mode, const rect_u16_t *region, render_result_t *out_result);
