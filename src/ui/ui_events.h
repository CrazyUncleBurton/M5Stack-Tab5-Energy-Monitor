#pragma once

#ifdef __cplusplus
extern "C" {
#endif

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

void on_reset_ch1(lv_event_t *e);
void on_reset_ch2(lv_event_t *e);
void on_reset_ch3(lv_event_t *e);

void on_nav_to_ch1(lv_event_t *e);
void on_nav_to_ch2(lv_event_t *e);
void on_nav_to_ch3(lv_event_t *e);

#ifdef __cplusplus
}
#endif
