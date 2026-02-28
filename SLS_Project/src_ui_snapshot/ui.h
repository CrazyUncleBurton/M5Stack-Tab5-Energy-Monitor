#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#if defined __has_include
#if __has_include("lvgl.h")
#include "lvgl.h"
#elif __has_include("lvgl/lvgl.h")
#include "lvgl/lvgl.h"
#else
#include "lvgl.h"
#endif
#else
#include "lvgl.h"
#endif

typedef struct {
    float voltage_v;
    float current_ma;
    float power_w;
    float energy_wh;
} ui_channel_data_t;

void ui_init(void);
void ui_set_channel_data(uint8_t channel, const ui_channel_data_t *data);
bool ui_consume_reset_request(uint8_t channel);

#ifdef __cplusplus
}
#endif
