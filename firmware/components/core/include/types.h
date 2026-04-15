#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint16_t width;
    uint16_t height;
} size_u16_t;

typedef struct {
    uint16_t x;
    uint16_t y;
} point_u16_t;

typedef struct {
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
} rect_u16_t;

typedef struct {
    bool ok;
    int32_t code;
    const char *message;
} op_result_t;
