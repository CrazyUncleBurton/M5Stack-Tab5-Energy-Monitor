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
    float load_temp_f;
} ui_channel_data_t;

typedef enum {
    UI_SENSOR_INA226_1A = 0,
    UI_SENSOR_INA226_10A = 1
} ui_sensor_type_t;

typedef enum {
    UI_UNITS_IMPERIAL = 0,
    UI_UNITS_METRIC = 1
} ui_units_t;

typedef enum {
    UI_BATTERY_ALKALINE = 0,
    UI_BATTERY_DRY_CELL = 1,
    UI_BATTERY_LEAD_ACID = 2,
    UI_BATTERY_SLA = 3,
    UI_BATTERY_AGM = 4,
    UI_BATTERY_NICD = 5,
    UI_BATTERY_NIMH = 6,
    UI_BATTERY_LITHIUM_PRIMARY = 7,
    UI_BATTERY_LITHIUM_ION = 8,
    UI_BATTERY_LIPO = 9,
    UI_BATTERY_LIFEPO4 = 10
} ui_battery_type_t;

typedef enum {
    UI_LOAD_CONSTANT_CURRENT = 0,
    UI_LOAD_CONSTANT_POWER = 1,
    UI_LOAD_CONSTANT_IMPEDANCE = 2,
    UI_LOAD_PULSED = 3
} ui_load_type_t;

typedef enum {
    UI_GRAPH_TRACE_VOLTAGE = (1u << 0),
    UI_GRAPH_TRACE_CURRENT = (1u << 1),
    UI_GRAPH_TRACE_POWER = (1u << 2),
    UI_GRAPH_TRACE_ENERGY = (1u << 3),
    UI_GRAPH_TRACE_LOAD_TEMP = (1u << 4),
    UI_GRAPH_TRACE_ALL = (UI_GRAPH_TRACE_VOLTAGE | UI_GRAPH_TRACE_CURRENT | UI_GRAPH_TRACE_POWER | UI_GRAPH_TRACE_ENERGY | UI_GRAPH_TRACE_LOAD_TEMP)
} ui_graph_trace_mask_t;

typedef struct {
    ui_sensor_type_t sensor_type;
    ui_units_t units;
    ui_battery_type_t battery_type;
    ui_load_type_t load_type;
    uint8_t graph_trace_mask;
    uint16_t sample_interval_ms;
    float cutoff_voltage_v;
    float overtemp_cutoff_c;
    float rated_battery_ampacity_ah;
    uint8_t num_series_cells;
} ui_config_t;

void ui_init(void);
void ui_set_channel_data(uint8_t channel, const ui_channel_data_t *data);
void ui_set_sensor_connected(bool connected);
void ui_set_sensor_status(const char *status_text);
bool ui_consume_start_request(uint8_t channel);
bool ui_consume_stop_request(uint8_t channel);
void ui_set_test_running(bool running);
bool ui_consume_reset_request(uint8_t channel);
void ui_request_reset(uint8_t channel);
void ui_load_channel_screen(uint8_t channel);
void ui_set_config(const ui_config_t *config);
bool ui_consume_config_update(ui_config_t *config);

#ifdef __cplusplus
}
#endif
