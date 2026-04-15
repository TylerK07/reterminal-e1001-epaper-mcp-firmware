#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "errors.h"

typedef struct {
    bool key0_pressed;
    bool key1_pressed;
    bool key2_pressed;
} button_state_t;

error_code_t buttons_init(void);
error_code_t buttons_get_state(button_state_t *out_state);
error_code_t buttons_is_any_pressed(bool *out_pressed);
