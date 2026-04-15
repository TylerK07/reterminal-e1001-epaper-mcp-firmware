#pragma once
#include "errors.h"
#include "render_models.h"

error_code_t render_service_init(void);
error_code_t render_service_render_text(const render_text_req_t *req, render_result_t *out_result);
error_code_t render_service_render_bitmap(const render_bitmap_req_t *req, render_result_t *out_result);
error_code_t render_service_render_layout(const render_layout_req_t *req, render_result_t *out_result);
error_code_t render_service_render_boot_screen(render_result_t *out_result);
error_code_t render_service_render_status_screen(const render_status_screen_t *screen, render_result_t *out_result);
error_code_t render_service_render_test_pattern(render_result_t *out_result);
error_code_t render_service_clear_region(const rect_u16_t *region, bool black, bool commit, display_refresh_mode_t refresh_mode, render_result_t *out_result);
error_code_t render_service_refresh(display_refresh_mode_t mode, const rect_u16_t *region, render_result_t *out_result);
error_code_t render_service_get_last_render_record(last_render_record_t *out_record);
