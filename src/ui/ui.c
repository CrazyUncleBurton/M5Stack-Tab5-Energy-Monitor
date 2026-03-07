#include "ui.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define METRIC_COUNT 5
#define HISTORY_MAX 900

typedef enum {
    METRIC_VOLTAGE = 0,
    METRIC_CURRENT = 1,
    METRIC_POWER = 2,
    METRIC_ENERGY = 3,
    METRIC_LOAD_TEMP = 4
} metric_t;

static lv_obj_t *screen_monitor;
static lv_obj_t *screen_config;

static lv_obj_t *value_voltage;
static lv_obj_t *value_current;
static lv_obj_t *value_power;
static lv_obj_t *value_energy;
static lv_obj_t *value_load_temp;
static lv_obj_t *sensor_status_label;

static lv_obj_t *chart_obj;
static lv_chart_series_t *chart_series[METRIC_COUNT];
static lv_obj_t *scale_label_voltage;
static lv_obj_t *scale_label_current;
static lv_obj_t *scale_label_power;
static lv_obj_t *scale_label_energy;
static lv_obj_t *scale_label_load_temp;

static lv_obj_t *dropdown_sensor;
static lv_obj_t *dropdown_units;
static lv_obj_t *dropdown_battery_type;
static lv_obj_t *dropdown_load_type;
static lv_obj_t *switch_graph_voltage;
static lv_obj_t *switch_graph_current;
static lv_obj_t *switch_graph_power;
static lv_obj_t *switch_graph_energy;
static lv_obj_t *switch_graph_load_temp;
static lv_obj_t *value_sample_interval;
static lv_obj_t *value_overtemp_cutoff;
static lv_obj_t *value_battery_ampacity;
static lv_obj_t *dropdown_series_cells;
static lv_obj_t *value_series_cells;
static lv_obj_t *value_pack_cutoff_preview;

static bool reset_requested = false;
static bool start_requested = false;
static bool stop_requested = false;
static bool config_update_pending = false;
static lv_obj_t *start_button_label = NULL;
static lv_obj_t *start_button_obj = NULL;
static lv_obj_t *stop_button_obj = NULL;

static uint16_t history_count = 0;
static uint32_t history_sample_counter = 0;
static uint16_t history_stride = 1;
static float history_raw[METRIC_COUNT][HISTORY_MAX];
static lv_coord_t history_chart[METRIC_COUNT][HISTORY_MAX];

static ui_channel_data_t latest_data;
static bool latest_data_valid = false;

static ui_config_t active_config = {
    .sensor_type = UI_SENSOR_INA226_1A,
    .units = UI_UNITS_IMPERIAL,
    .battery_type = UI_BATTERY_LIFEPO4,
    .load_type = UI_LOAD_CONSTANT_CURRENT,
    .graph_trace_mask = UI_GRAPH_TRACE_ALL,
    .sample_interval_ms = 200,
    .cutoff_voltage_v = 10.0f,
    .overtemp_cutoff_c = 60.0f,
    .rated_battery_ampacity_ah = 100.0f,
    .num_series_cells = 4
};

static ui_config_t pending_config;

static lv_style_t style_screen;
static lv_style_t style_header;
static lv_style_t style_section;
static lv_style_t style_channel_title;
static lv_style_t style_metric_label;
static lv_style_t style_metric_value;
static lv_style_t style_button;
static lv_style_t style_button_text;
static lv_style_t style_status_text;
static lv_style_t style_nav_button;
static lv_style_t style_nav_button_active;

static void set_value_text(lv_obj_t *label, float value, uint8_t decimals, const char *unit);
static void set_temp_text(lv_obj_t *label, float value);
static void refresh_chart(void);
static void refresh_config_values(void);
static void apply_chart_visibility(void);

static void format_fixed(char *out, size_t out_size, float value, uint8_t decimals)
{
    bool negative = value < 0.0f;
    if (negative) {
        value = -value;
    }

    int32_t scale = 1;
    for (uint8_t i = 0; i < decimals; i++) {
        scale *= 10;
    }

    int32_t scaled = (int32_t)(value * (float)scale + 0.5f);
    int32_t whole = scaled / scale;
    int32_t frac = scaled % scale;

    if (decimals == 0) {
        snprintf(out, out_size, "%s%ld", negative ? "-" : "", (long)whole);
        return;
    }

    snprintf(out, out_size, "%s%ld.%0*ld", negative ? "-" : "", (long)whole, decimals, (long)frac);
}

static void set_value_text(lv_obj_t *label, float value, uint8_t decimals, const char *unit)
{
    char number[24];
    char text[32];

    format_fixed(number, sizeof(number), value, decimals);
    snprintf(text, sizeof(text), "%s %s", number, unit);
    lv_label_set_text(label, text);
}

static void set_temp_text(lv_obj_t *label, float value)
{
    if (label == NULL) {
        return;
    }

    if (isnan(value)) {
        lv_label_set_text(label, active_config.units == UI_UNITS_METRIC ? "--.- C" : "--.- F");
        return;
    }

    set_value_text(label, value, 1, active_config.units == UI_UNITS_METRIC ? "C" : "F");
}

static void apply_values(const ui_channel_data_t *data)
{
    set_value_text(value_voltage, data->voltage_v, 3, "V");
    set_value_text(value_current, data->current_ma, 2, "mA");
    set_value_text(value_power, data->power_w, 3, "W");
    set_value_text(value_energy, data->energy_wh, 4, "Wh");
    set_temp_text(value_load_temp, data->load_temp_f);
}

static void clear_history(void)
{
    history_count = 0;
    history_sample_counter = 0;
    history_stride = 1;
    for (uint8_t metric = 0; metric < METRIC_COUNT; metric++) {
        memset(history_raw[metric], 0, sizeof(history_raw[metric]));
        memset(history_chart[metric], 0, sizeof(history_chart[metric]));
    }
    refresh_chart();
}

static void compress_history(void)
{
    if (history_count < 2) {
        return;
    }

    for (uint8_t metric = 0; metric < METRIC_COUNT; metric++) {
        uint16_t dst = 0;
        for (uint16_t src = 0; src < history_count; src += 2) {
            float r0 = history_raw[metric][src];
            float r1 = (src + 1 < history_count) ? history_raw[metric][src + 1] : r0;
            history_raw[metric][dst] = (r0 + r1) * 0.5f;

            lv_coord_t v0 = history_chart[metric][src];
            lv_coord_t v1 = (src + 1 < history_count) ? history_chart[metric][src + 1] : v0;
            history_chart[metric][dst++] = (lv_coord_t)((v0 + v1) / 2);
        }
    }

    history_count = (uint16_t)((history_count + 1) / 2);
    history_stride = (uint16_t)(history_stride * 2);
}

static void refresh_chart(void)
{
    if (chart_obj == NULL) {
        return;
    }

    if (history_count == 0) {
        lv_chart_set_point_count(chart_obj, 1);
        for (uint8_t metric = 0; metric < METRIC_COUNT; metric++) {
            history_chart[metric][0] = 0;
        }
        lv_chart_set_range(chart_obj, LV_CHART_AXIS_PRIMARY_Y, 0, 1);
        lv_chart_refresh(chart_obj);
        return;
    }

    lv_chart_set_point_count(chart_obj, history_count);
    lv_chart_set_div_line_count(chart_obj, 9, 8);

    for (uint8_t metric = 0; metric < METRIC_COUNT; metric++) {
        float minv = history_raw[metric][0];
        float maxv = history_raw[metric][0];

        for (uint16_t i = 1; i < history_count; i++) {
            float v = history_raw[metric][i];
            if (v < minv) {
                minv = v;
            }
            if (v > maxv) {
                maxv = v;
            }
        }

        float center = (minv + maxv) * 0.5f;
        float half_span = (maxv - minv) * 0.5f;
        if (half_span < 0.0001f) {
            float abs_center = center < 0.0f ? -center : center;
            half_span = (abs_center * 0.1f);
            if (half_span < 0.01f) {
                half_span = 0.01f;
            }
        }

        for (uint16_t i = 0; i < history_count; i++) {
            float normalized = (history_raw[metric][i] - center) / half_span;
            if (normalized > 1.2f) {
                normalized = 1.2f;
            } else if (normalized < -1.2f) {
                normalized = -1.2f;
            }
            history_chart[metric][i] = (lv_coord_t)(normalized * 900.0f);
        }
    }

    lv_chart_set_range(chart_obj, LV_CHART_AXIS_PRIMARY_Y, -1000, 1000);
    for (uint8_t metric = 0; metric < METRIC_COUNT; metric++) {
        lv_chart_set_x_start_point(chart_obj, chart_series[metric], 0);
    }
    lv_chart_refresh(chart_obj);
}

static lv_obj_t *create_metric_row(lv_obj_t *parent, const char *name, lv_color_t color, lv_obj_t **value_label)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 2, LV_PART_MAIN);

    lv_obj_t *label_name = lv_label_create(row);
    lv_label_set_text(label_name, name);
    lv_obj_set_width(label_name, lv_pct(48));
    lv_obj_add_style(label_name, &style_metric_label, LV_PART_MAIN);
    lv_obj_set_style_text_color(label_name, color, LV_PART_MAIN);
    lv_obj_set_style_text_align(label_name, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);

    lv_obj_t *label_colon = lv_label_create(row);
    lv_label_set_text(label_colon, ":");
    lv_obj_set_width(label_colon, 12);
    lv_obj_add_style(label_colon, &style_metric_label, LV_PART_MAIN);
    lv_obj_set_style_text_color(label_colon, color, LV_PART_MAIN);
    lv_obj_set_style_text_align(label_colon, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    *value_label = lv_label_create(row);
    lv_label_set_text(*value_label, "0");
    lv_obj_set_width(*value_label, lv_pct(46));
    lv_obj_add_style(*value_label, &style_metric_value, LV_PART_MAIN);
    lv_obj_set_style_text_color(*value_label, color, LV_PART_MAIN);
    lv_obj_set_style_text_align(*value_label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);

    return row;
}

static lv_obj_t *create_config_row(lv_obj_t *parent, const char *name)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_add_style(row, &style_section, LV_PART_MAIN);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(row, 0, LV_PART_MAIN);

    lv_obj_t *label = lv_label_create(row);
    lv_label_set_text(label, name);
    lv_obj_add_style(label, &style_metric_label, LV_PART_MAIN);

    return row;
}

static lv_obj_t *create_small_button(lv_obj_t *parent, const char *text)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 54, 46);
    lv_obj_add_style(btn, &style_nav_button, LV_PART_MAIN);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_add_style(label, &style_button_text, LV_PART_MAIN);
    lv_obj_center(label);
    return btn;
}

static void create_chart_index_item(lv_obj_t *parent, const char *name, lv_color_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, name);
    lv_obj_add_style(label, &style_status_text, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
}

static lv_obj_t *create_chart_scale_item(lv_obj_t *parent, const char *name, lv_color_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, name);
    lv_obj_set_width(label, lv_pct(100));
    lv_obj_set_height(label, 40);
    lv_obj_add_style(label, &style_status_text, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(label, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_left(label, 8, LV_PART_MAIN);
    return label;
}

static void toggle_graph_trace(const uint8_t bit)
{
    uint8_t new_mask = active_config.graph_trace_mask;
    if ((new_mask & bit) != 0) {
        new_mask &= (uint8_t)(~bit);
    } else {
        new_mask |= bit;
    }

    if (new_mask == 0) {
        return;
    }

    active_config.graph_trace_mask = new_mask;
    pending_config.graph_trace_mask = new_mask;
    apply_chart_visibility();
    config_update_pending = true;
    refresh_config_values();
}

static void on_scale_voltage_clicked(lv_event_t *e)
{
    (void)e;
    toggle_graph_trace(UI_GRAPH_TRACE_VOLTAGE);
}

static void on_scale_current_clicked(lv_event_t *e)
{
    (void)e;
    toggle_graph_trace(UI_GRAPH_TRACE_CURRENT);
}

static void on_scale_power_clicked(lv_event_t *e)
{
    (void)e;
    toggle_graph_trace(UI_GRAPH_TRACE_POWER);
}

static void on_scale_energy_clicked(lv_event_t *e)
{
    (void)e;
    toggle_graph_trace(UI_GRAPH_TRACE_ENERGY);
}

static void on_scale_temp_clicked(lv_event_t *e)
{
    (void)e;
    toggle_graph_trace(UI_GRAPH_TRACE_LOAD_TEMP);
}

static void set_switch_checked(lv_obj_t *sw, bool checked)
{
    if (sw == NULL) {
        return;
    }
    if (checked) {
        lv_obj_add_state(sw, LV_STATE_CHECKED);
    } else {
        lv_obj_clear_state(sw, LV_STATE_CHECKED);
    }
}

static void apply_chart_visibility(void)
{
    if (chart_obj == NULL) {
        return;
    }

    lv_chart_hide_series(chart_obj, chart_series[METRIC_VOLTAGE], (active_config.graph_trace_mask & UI_GRAPH_TRACE_VOLTAGE) == 0);
    lv_chart_hide_series(chart_obj, chart_series[METRIC_CURRENT], (active_config.graph_trace_mask & UI_GRAPH_TRACE_CURRENT) == 0);
    lv_chart_hide_series(chart_obj, chart_series[METRIC_POWER], (active_config.graph_trace_mask & UI_GRAPH_TRACE_POWER) == 0);
    lv_chart_hide_series(chart_obj, chart_series[METRIC_ENERGY], (active_config.graph_trace_mask & UI_GRAPH_TRACE_ENERGY) == 0);
    lv_chart_hide_series(chart_obj, chart_series[METRIC_LOAD_TEMP], (active_config.graph_trace_mask & UI_GRAPH_TRACE_LOAD_TEMP) == 0);

    if (scale_label_voltage != NULL) {
        lv_obj_set_style_text_opa(scale_label_voltage,
                                  (active_config.graph_trace_mask & UI_GRAPH_TRACE_VOLTAGE) ? LV_OPA_COVER : LV_OPA_40,
                                  LV_PART_MAIN);
    }
    if (scale_label_current != NULL) {
        lv_obj_set_style_text_opa(scale_label_current,
                                  (active_config.graph_trace_mask & UI_GRAPH_TRACE_CURRENT) ? LV_OPA_COVER : LV_OPA_40,
                                  LV_PART_MAIN);
    }
    if (scale_label_power != NULL) {
        lv_obj_set_style_text_opa(scale_label_power,
                                  (active_config.graph_trace_mask & UI_GRAPH_TRACE_POWER) ? LV_OPA_COVER : LV_OPA_40,
                                  LV_PART_MAIN);
    }
    if (scale_label_energy != NULL) {
        lv_obj_set_style_text_opa(scale_label_energy,
                                  (active_config.graph_trace_mask & UI_GRAPH_TRACE_ENERGY) ? LV_OPA_COVER : LV_OPA_40,
                                  LV_PART_MAIN);
    }
    if (scale_label_load_temp != NULL) {
        lv_obj_set_style_text_opa(scale_label_load_temp,
                                  (active_config.graph_trace_mask & UI_GRAPH_TRACE_LOAD_TEMP) ? LV_OPA_COVER : LV_OPA_40,
                                  LV_PART_MAIN);
    }

    lv_chart_refresh(chart_obj);
}

static float battery_cutoff_per_cell(const ui_battery_type_t type)
{
    switch (type) {
        case UI_BATTERY_ALKALINE:
        case UI_BATTERY_DRY_CELL:
            return 0.9f;
        case UI_BATTERY_LEAD_ACID:
        case UI_BATTERY_SLA:
            return 1.93f;
        case UI_BATTERY_AGM:
            return 1.75f;
        case UI_BATTERY_NICD:
        case UI_BATTERY_NIMH:
            return 1.2f;
        case UI_BATTERY_LITHIUM_PRIMARY:
            return 2.0f;
        case UI_BATTERY_LITHIUM_ION:
        case UI_BATTERY_LIPO:
            return 3.5f;
        case UI_BATTERY_LIFEPO4:
        default:
            return 2.5f;
    }
}

static void refresh_config_values(void)
{
    if (dropdown_sensor != NULL) {
        lv_dropdown_set_selected(dropdown_sensor, (uint16_t)pending_config.sensor_type);
    }
    if (dropdown_units != NULL) {
        lv_dropdown_set_selected(dropdown_units, (uint16_t)pending_config.units);
    }
    if (dropdown_battery_type != NULL) {
        lv_dropdown_set_selected(dropdown_battery_type, (uint16_t)pending_config.battery_type);
    }
    if (dropdown_load_type != NULL) {
        lv_dropdown_set_selected(dropdown_load_type, (uint16_t)pending_config.load_type);
    }
    if (value_sample_interval != NULL) {
        char buffer[24];
        snprintf(buffer, sizeof(buffer), "%u ms", (unsigned)pending_config.sample_interval_ms);
        lv_label_set_text(value_sample_interval, buffer);
    }

    if (value_overtemp_cutoff != NULL) {
        char number[24];
        char buffer[32];
        float display_temp = pending_config.overtemp_cutoff_c;
        const char *unit = "C";
        if (pending_config.units == UI_UNITS_IMPERIAL) {
            display_temp = (display_temp * 9.0f / 5.0f) + 32.0f;
            unit = "F";
        }
        format_fixed(number, sizeof(number), display_temp, 0);
        snprintf(buffer, sizeof(buffer), "%s %s", number, unit);
        lv_label_set_text(value_overtemp_cutoff, buffer);
    }

    if (value_battery_ampacity != NULL) {
        char number[24];
        char buffer[32];
        format_fixed(number, sizeof(number), pending_config.rated_battery_ampacity_ah, 1);
        snprintf(buffer, sizeof(buffer), "%s Ah", number);
        lv_label_set_text(value_battery_ampacity, buffer);
    }

    if (dropdown_series_cells != NULL) {
        lv_dropdown_set_selected(dropdown_series_cells, pending_config.num_series_cells - 1);
    }
    if (value_pack_cutoff_preview != NULL) {
        char number_pack[24];
        char number_cell[24];
        char buffer[64];
        float per_cell = battery_cutoff_per_cell(pending_config.battery_type);
        float pack_cutoff = per_cell * (float)pending_config.num_series_cells;
        format_fixed(number_pack, sizeof(number_pack), pack_cutoff, 2);
        format_fixed(number_cell, sizeof(number_cell), per_cell, 2);
        snprintf(buffer, sizeof(buffer), "%s V (%sV x %uS)", number_pack,
                 number_cell, (unsigned)pending_config.num_series_cells);
        lv_label_set_text(value_pack_cutoff_preview, buffer);
    }
}

static void on_start_clicked(lv_event_t *e)
{
    (void)e;
    clear_history();
    start_requested = true;
}

static void on_stop_clicked(lv_event_t *e)
{
    (void)e;
    stop_requested = true;
}

static void on_open_config_clicked(lv_event_t *e)
{
    (void)e;
    pending_config = active_config;
    refresh_config_values();
    lv_disp_load_scr(screen_config);
}

static void on_back_clicked(lv_event_t *e)
{
    (void)e;
    lv_disp_load_scr(screen_monitor);
}

static void on_apply_clicked(lv_event_t *e)
{
    (void)e;
    if (pending_config.graph_trace_mask == 0) {
        pending_config.graph_trace_mask = UI_GRAPH_TRACE_ALL;
    }
    active_config = pending_config;
    apply_chart_visibility();
    config_update_pending = true;
    if (latest_data_valid) {
        apply_values(&latest_data);
    }
    lv_disp_load_scr(screen_monitor);
}

static void on_graph_voltage_changed(lv_event_t *e)
{
    (void)e;
    bool checked = lv_obj_has_state(switch_graph_voltage, LV_STATE_CHECKED);
    if (checked) {
        pending_config.graph_trace_mask |= UI_GRAPH_TRACE_VOLTAGE;
    } else {
        pending_config.graph_trace_mask &= (uint8_t)(~UI_GRAPH_TRACE_VOLTAGE);
    }
}

static void on_graph_current_changed(lv_event_t *e)
{
    (void)e;
    bool checked = lv_obj_has_state(switch_graph_current, LV_STATE_CHECKED);
    if (checked) {
        pending_config.graph_trace_mask |= UI_GRAPH_TRACE_CURRENT;
    } else {
        pending_config.graph_trace_mask &= (uint8_t)(~UI_GRAPH_TRACE_CURRENT);
    }
}

static void on_graph_power_changed(lv_event_t *e)
{
    (void)e;
    bool checked = lv_obj_has_state(switch_graph_power, LV_STATE_CHECKED);
    if (checked) {
        pending_config.graph_trace_mask |= UI_GRAPH_TRACE_POWER;
    } else {
        pending_config.graph_trace_mask &= (uint8_t)(~UI_GRAPH_TRACE_POWER);
    }
}

static void on_graph_energy_changed(lv_event_t *e)
{
    (void)e;
    bool checked = lv_obj_has_state(switch_graph_energy, LV_STATE_CHECKED);
    if (checked) {
        pending_config.graph_trace_mask |= UI_GRAPH_TRACE_ENERGY;
    } else {
        pending_config.graph_trace_mask &= (uint8_t)(~UI_GRAPH_TRACE_ENERGY);
    }
}

static void on_graph_load_temp_changed(lv_event_t *e)
{
    (void)e;
    bool checked = lv_obj_has_state(switch_graph_load_temp, LV_STATE_CHECKED);
    if (checked) {
        pending_config.graph_trace_mask |= UI_GRAPH_TRACE_LOAD_TEMP;
    } else {
        pending_config.graph_trace_mask &= (uint8_t)(~UI_GRAPH_TRACE_LOAD_TEMP);
    }
}

static void on_sensor_changed(lv_event_t *e)
{
    (void)e;
    pending_config.sensor_type = (ui_sensor_type_t)lv_dropdown_get_selected(dropdown_sensor);
}

static void on_units_changed(lv_event_t *e)
{
    (void)e;
    pending_config.units = (ui_units_t)lv_dropdown_get_selected(dropdown_units);
    refresh_config_values();
}

static void on_sample_minus(lv_event_t *e)
{
    (void)e;
    if (pending_config.sample_interval_ms > 100) {
        pending_config.sample_interval_ms -= 100;
    }
    refresh_config_values();
}

static void on_sample_plus(lv_event_t *e)
{
    (void)e;
    if (pending_config.sample_interval_ms < 2000) {
        pending_config.sample_interval_ms += 100;
    }
    refresh_config_values();
}

static void on_battery_type_changed(lv_event_t *e)
{
    (void)e;
    pending_config.battery_type = (ui_battery_type_t)lv_dropdown_get_selected(dropdown_battery_type);

    pending_config.cutoff_voltage_v = battery_cutoff_per_cell(pending_config.battery_type) * (float)pending_config.num_series_cells;
    refresh_config_values();
}

static void on_battery_ampacity_changed(lv_event_t *e)
{
    (void)e;
    // This function is no longer used with +/- buttons
}

static void on_ampacity_minus(lv_event_t *e)
{
    (void)e;
    pending_config.rated_battery_ampacity_ah -= 0.1f;
    if (pending_config.rated_battery_ampacity_ah < 0.1f) {
        pending_config.rated_battery_ampacity_ah = 0.1f;
    }
    refresh_config_values();
}

static void on_ampacity_plus(lv_event_t *e)
{
    (void)e;
    pending_config.rated_battery_ampacity_ah += 0.1f;
    if (pending_config.rated_battery_ampacity_ah > 500.0f) {
        pending_config.rated_battery_ampacity_ah = 500.0f;
    }
    refresh_config_values();
}

static void on_load_type_changed(lv_event_t *e)
{
    (void)e;
    pending_config.load_type = (ui_load_type_t)lv_dropdown_get_selected(dropdown_load_type);
}

static void on_overtemp_minus(lv_event_t *e)
{
    (void)e;
    if (pending_config.units == UI_UNITS_METRIC) {
        pending_config.overtemp_cutoff_c -= 5.0f;
    } else {
        pending_config.overtemp_cutoff_c -= (5.0f / 1.8f);
    }
    if (pending_config.overtemp_cutoff_c < 30.0f) {
        pending_config.overtemp_cutoff_c = 30.0f;
    }
    refresh_config_values();
}

static void on_overtemp_plus(lv_event_t *e)
{
    (void)e;
    if (pending_config.units == UI_UNITS_METRIC) {
        pending_config.overtemp_cutoff_c += 5.0f;
    } else {
        pending_config.overtemp_cutoff_c += (5.0f / 1.8f);
    }
    if (pending_config.overtemp_cutoff_c > 100.0f) {
        pending_config.overtemp_cutoff_c = 100.0f;
    }
    refresh_config_values();
}

static void on_series_cells_changed(lv_event_t *e)
{
    (void)e;
    pending_config.num_series_cells = (uint8_t)(lv_dropdown_get_selected(dropdown_series_cells) + 1);

    pending_config.cutoff_voltage_v = battery_cutoff_per_cell(pending_config.battery_type) * (float)pending_config.num_series_cells;
    refresh_config_values();
}

static void init_styles(void)
{
    lv_style_init(&style_screen);
    lv_style_set_bg_color(&style_screen, lv_color_hex(0x0B1020));
    lv_style_set_bg_opa(&style_screen, LV_OPA_COVER);
    lv_style_set_pad_all(&style_screen, 12);

    lv_style_init(&style_header);
    lv_style_set_text_color(&style_header, lv_color_hex(0xEAF1FF));
    lv_style_set_text_font(&style_header, &lv_font_montserrat_24);
    lv_style_set_text_align(&style_header, LV_TEXT_ALIGN_CENTER);

    lv_style_init(&style_section);
    lv_style_set_bg_color(&style_section, lv_color_hex(0x151D35));
    lv_style_set_bg_opa(&style_section, LV_OPA_COVER);
    lv_style_set_border_color(&style_section, lv_color_hex(0x2E3C66));
    lv_style_set_border_width(&style_section, 2);
    lv_style_set_radius(&style_section, 12);
    lv_style_set_pad_all(&style_section, 8);

    lv_style_init(&style_channel_title);
    lv_style_set_text_color(&style_channel_title, lv_color_hex(0xF7FAFF));
    lv_style_set_text_font(&style_channel_title, &lv_font_montserrat_32);
    lv_style_set_text_align(&style_channel_title, LV_TEXT_ALIGN_CENTER);

    lv_style_init(&style_metric_label);
    lv_style_set_text_color(&style_metric_label, lv_color_hex(0xB8C6EC));
    lv_style_set_text_font(&style_metric_label, &lv_font_montserrat_20);

    lv_style_init(&style_metric_value);
    lv_style_set_text_color(&style_metric_value, lv_color_hex(0xFFFFFF));
    lv_style_set_text_font(&style_metric_value, &lv_font_montserrat_24);

    lv_style_init(&style_button);
    lv_style_set_radius(&style_button, 10);
    lv_style_set_bg_color(&style_button, lv_palette_main(LV_PALETTE_RED));
    lv_style_set_bg_opa(&style_button, LV_OPA_COVER);
    lv_style_set_text_color(&style_button, lv_color_white());
    lv_style_set_pad_ver(&style_button, 12);
    lv_style_set_pad_hor(&style_button, 16);

    lv_style_init(&style_button_text);
    lv_style_set_text_font(&style_button_text, &lv_font_montserrat_20);

    lv_style_init(&style_status_text);
    lv_style_set_text_font(&style_status_text, &lv_font_montserrat_18);
    lv_style_set_text_color(&style_status_text, lv_color_hex(0xFFECEC));
    lv_style_set_text_align(&style_status_text, LV_TEXT_ALIGN_CENTER);

    lv_style_init(&style_nav_button);
    lv_style_set_radius(&style_nav_button, 9);
    lv_style_set_bg_color(&style_nav_button, lv_color_hex(0x26375E));
    lv_style_set_bg_opa(&style_nav_button, LV_OPA_COVER);
    lv_style_set_text_color(&style_nav_button, lv_color_white());
    lv_style_set_pad_ver(&style_nav_button, 10);
    lv_style_set_pad_hor(&style_nav_button, 14);

    lv_style_init(&style_nav_button_active);
    lv_style_set_bg_color(&style_nav_button_active, lv_color_hex(0x3A5EA8));
}

static void build_monitor_screen(void)
{
    screen_monitor = lv_obj_create(NULL);
    lv_obj_remove_style_all(screen_monitor);
    lv_obj_set_size(screen_monitor, LV_PCT(100), LV_PCT(100));
    lv_obj_add_style(screen_monitor, &style_screen, LV_PART_MAIN);
    lv_obj_set_flex_flow(screen_monitor, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(screen_monitor, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(screen_monitor, 6, LV_PART_MAIN);

    lv_obj_t *header = lv_label_create(screen_monitor);
    lv_label_set_text(header, "CrazyUncleBurton.com Energy Monitor");
    lv_obj_set_width(header, lv_pct(100));
    lv_obj_add_style(header, &style_header, LV_PART_MAIN);

    lv_obj_t *chart_card = lv_obj_create(screen_monitor);
    lv_obj_set_size(chart_card, lv_pct(100), lv_pct(42));
    lv_obj_set_flex_grow(chart_card, 1);
    lv_obj_add_style(chart_card, &style_section, LV_PART_MAIN);
    lv_obj_set_flex_flow(chart_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(chart_card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(chart_card, 6, LV_PART_MAIN);

    lv_obj_t *chart_row = lv_obj_create(chart_card);
    lv_obj_remove_style_all(chart_row);
    lv_obj_set_size(chart_row, lv_pct(100), lv_pct(82));
    lv_obj_set_flex_flow(chart_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(chart_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(chart_row, 8, LV_PART_MAIN);

    lv_obj_t *scale_col = lv_obj_create(chart_row);
    lv_obj_remove_style_all(scale_col);
    lv_obj_set_size(scale_col, lv_pct(20), lv_pct(100));
    lv_obj_set_flex_flow(scale_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scale_col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(scale_col, 14, LV_PART_MAIN);

    scale_label_voltage = create_chart_scale_item(scale_col, "V", lv_palette_main(LV_PALETTE_RED));
    scale_label_current = create_chart_scale_item(scale_col, "mA", lv_palette_main(LV_PALETTE_YELLOW));
    scale_label_power = create_chart_scale_item(scale_col, "W", lv_palette_main(LV_PALETTE_GREEN));
    scale_label_energy = create_chart_scale_item(scale_col, "Wh", lv_palette_main(LV_PALETTE_BLUE));
    scale_label_load_temp = create_chart_scale_item(scale_col, active_config.units == UI_UNITS_METRIC ? "C" : "F", lv_palette_main(LV_PALETTE_CYAN));

    lv_obj_add_flag(scale_label_voltage, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(scale_label_current, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(scale_label_power, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(scale_label_energy, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(scale_label_load_temp, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_add_event_cb(scale_label_voltage, on_scale_voltage_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(scale_label_current, on_scale_current_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(scale_label_power, on_scale_power_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(scale_label_energy, on_scale_energy_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(scale_label_load_temp, on_scale_temp_clicked, LV_EVENT_CLICKED, NULL);

    chart_obj = lv_chart_create(chart_row);
    lv_obj_set_size(chart_obj, lv_pct(80), lv_pct(100));
    lv_chart_set_type(chart_obj, LV_CHART_TYPE_LINE);
    lv_chart_set_div_line_count(chart_obj, 9, 8);
    lv_chart_set_range(chart_obj, LV_CHART_AXIS_PRIMARY_Y, -1000, 1000);
    lv_chart_set_point_count(chart_obj, 1);
    lv_obj_set_style_bg_color(chart_obj, lv_color_hex(0x0F1528), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(chart_obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(chart_obj, lv_color_hex(0x32456F), LV_PART_MAIN);
    lv_obj_set_style_border_width(chart_obj, 2, LV_PART_MAIN);
    lv_obj_set_style_line_color(chart_obj, lv_color_hex(0x4A5D89), LV_PART_MAIN);
    lv_obj_set_style_line_width(chart_obj, 1, LV_PART_MAIN);
    lv_obj_set_style_line_opa(chart_obj, LV_OPA_70, LV_PART_MAIN);
    lv_obj_set_style_pad_all(chart_obj, 6, LV_PART_MAIN);
    lv_obj_set_style_line_width(chart_obj, 2, LV_PART_ITEMS);

    chart_series[METRIC_VOLTAGE] = lv_chart_add_series(chart_obj, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);
    chart_series[METRIC_CURRENT] = lv_chart_add_series(chart_obj, lv_palette_main(LV_PALETTE_YELLOW), LV_CHART_AXIS_PRIMARY_Y);
    chart_series[METRIC_POWER] = lv_chart_add_series(chart_obj, lv_palette_main(LV_PALETTE_GREEN), LV_CHART_AXIS_PRIMARY_Y);
    chart_series[METRIC_ENERGY] = lv_chart_add_series(chart_obj, lv_palette_main(LV_PALETTE_BLUE), LV_CHART_AXIS_PRIMARY_Y);
    chart_series[METRIC_LOAD_TEMP] = lv_chart_add_series(chart_obj, lv_palette_main(LV_PALETTE_CYAN), LV_CHART_AXIS_PRIMARY_Y);

    for (uint8_t metric = 0; metric < METRIC_COUNT; metric++) {
        lv_chart_set_x_start_point(chart_obj, chart_series[metric], 0);
        lv_chart_set_ext_y_array(chart_obj, chart_series[metric], history_chart[metric]);
    }
    apply_chart_visibility();

    lv_obj_t *index_row = lv_obj_create(chart_card);
    lv_obj_remove_style_all(index_row);
    lv_obj_set_width(index_row, lv_pct(100));
    lv_obj_set_height(index_row, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(index_row, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(index_row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(index_row, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_row(index_row, 2, LV_PART_MAIN);

    create_chart_index_item(index_row, "Voltage", lv_palette_main(LV_PALETTE_RED));
    create_chart_index_item(index_row, "Current", lv_palette_main(LV_PALETTE_YELLOW));
    create_chart_index_item(index_row, "Power", lv_palette_main(LV_PALETTE_GREEN));
    create_chart_index_item(index_row, "Energy", lv_palette_main(LV_PALETTE_BLUE));
    create_chart_index_item(index_row, "Load Temp", lv_palette_main(LV_PALETTE_CYAN));

    lv_obj_t *instant = lv_obj_create(screen_monitor);
    lv_obj_set_size(instant, lv_pct(100), lv_pct(30));
    lv_obj_add_style(instant, &style_section, LV_PART_MAIN);
    lv_obj_set_flex_flow(instant, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(instant, 6, LV_PART_MAIN);

    lv_obj_t *channel_title = lv_label_create(instant);
    lv_label_set_text(channel_title, "Energy Test Data");
    lv_obj_set_width(channel_title, lv_pct(100));
    lv_obj_add_style(channel_title, &style_channel_title, LV_PART_MAIN);

    lv_obj_t *columns = lv_obj_create(instant);
    lv_obj_remove_style_all(columns);
    lv_obj_set_size(columns, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(columns, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(columns, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(columns, 10, LV_PART_MAIN);

    lv_obj_t *metrics_col = lv_obj_create(columns);
    lv_obj_remove_style_all(metrics_col);
    lv_obj_set_width(metrics_col, lv_pct(100));
    lv_obj_set_height(metrics_col, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(metrics_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(metrics_col, 3, LV_PART_MAIN);

    create_metric_row(metrics_col, "Voltage", lv_palette_main(LV_PALETTE_RED), &value_voltage);
    create_metric_row(metrics_col, "Current", lv_palette_main(LV_PALETTE_YELLOW), &value_current);
    create_metric_row(metrics_col, "Power", lv_palette_main(LV_PALETTE_GREEN), &value_power);
    create_metric_row(metrics_col, "Total Energy", lv_palette_main(LV_PALETTE_BLUE), &value_energy);
    create_metric_row(metrics_col, "Load Temp", lv_palette_main(LV_PALETTE_CYAN), &value_load_temp);

    sensor_status_label = lv_label_create(metrics_col);
    lv_label_set_text(sensor_status_label,
                      "INA226-1A: --\n"
                      "INA226-10A: --\n"
                      "Load Thermocouple: --\n"
                      "DAC2: --");
    lv_obj_set_width(sensor_status_label, lv_pct(100));
    lv_obj_add_style(sensor_status_label, &style_status_text, LV_PART_MAIN);
    lv_obj_set_style_text_align(sensor_status_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_pad_top(sensor_status_label, 10, LV_PART_MAIN);

    lv_obj_t *footer = lv_obj_create(screen_monitor);
    lv_obj_set_size(footer, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_add_style(footer, &style_section, LV_PART_MAIN);
    lv_obj_set_flex_flow(footer, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(footer, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    start_button_obj = lv_btn_create(footer);
    lv_obj_set_size(start_button_obj, 220, 64);
    lv_obj_add_style(start_button_obj, &style_nav_button_active, LV_PART_MAIN);
    lv_obj_set_style_bg_color(start_button_obj, lv_palette_main(LV_PALETTE_GREEN), LV_PART_MAIN);
    lv_obj_add_event_cb(start_button_obj, on_start_clicked, LV_EVENT_CLICKED, NULL);

    start_button_label = lv_label_create(start_button_obj);
    lv_label_set_text(start_button_label, "Start Test");
    lv_obj_add_style(start_button_label, &style_button_text, LV_PART_MAIN);
    lv_obj_center(start_button_label);

    stop_button_obj = lv_btn_create(footer);
    lv_obj_set_size(stop_button_obj, 220, 64);
    lv_obj_add_style(stop_button_obj, &style_button, LV_PART_MAIN);
    lv_obj_add_event_cb(stop_button_obj, on_stop_clicked, LV_EVENT_CLICKED, NULL);

    lv_obj_t *stop_label = lv_label_create(stop_button_obj);
    lv_label_set_text(stop_label, "Stop Test");
    lv_obj_add_style(stop_label, &style_button_text, LV_PART_MAIN);
    lv_obj_center(stop_label);

    lv_obj_t *config_btn = lv_btn_create(footer);
    lv_obj_set_size(config_btn, 220, 64);
    lv_obj_add_style(config_btn, &style_nav_button_active, LV_PART_MAIN);
    lv_obj_add_event_cb(config_btn, on_open_config_clicked, LV_EVENT_CLICKED, NULL);

    lv_obj_t *config_label = lv_label_create(config_btn);
    lv_label_set_text(config_label, "Config");
    lv_obj_add_style(config_label, &style_button_text, LV_PART_MAIN);
    lv_obj_center(config_label);

    clear_history();
}

static void build_config_screen(void)
{
    screen_config = lv_obj_create(NULL);
    lv_obj_remove_style_all(screen_config);
    lv_obj_set_size(screen_config, LV_PCT(100), LV_PCT(100));
    lv_obj_add_style(screen_config, &style_screen, LV_PART_MAIN);
    lv_obj_set_flex_flow(screen_config, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(screen_config, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(screen_config, 8, LV_PART_MAIN);

    lv_obj_t *header = lv_label_create(screen_config);
    lv_label_set_text(header, "Energy Test Config");
    lv_obj_set_width(header, lv_pct(100));
    lv_obj_add_style(header, &style_header, LV_PART_MAIN);

    lv_obj_t *list = lv_obj_create(screen_config);
    lv_obj_set_size(list, lv_pct(100), lv_pct(72));
    lv_obj_add_style(list, &style_section, LV_PART_MAIN);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(list, 8, LV_PART_MAIN);

    lv_obj_t *row_sensor = create_config_row(list, "Current Sensor");
    dropdown_sensor = lv_dropdown_create(row_sensor);
    lv_dropdown_set_options(dropdown_sensor, "INA226-1A\nINA226-10A");
    lv_obj_set_width(dropdown_sensor, 260);
    lv_obj_add_event_cb(dropdown_sensor, on_sensor_changed, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *row_units = create_config_row(list, "Units");
    dropdown_units = lv_dropdown_create(row_units);
    lv_dropdown_set_options(dropdown_units, "Imperial\nMetric");
    lv_obj_set_width(dropdown_units, 260);
    lv_obj_add_event_cb(dropdown_units, on_units_changed, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *row_battery_type = create_config_row(list, "Battery Type");
    dropdown_battery_type = lv_dropdown_create(row_battery_type);
    lv_dropdown_set_options(dropdown_battery_type, 
        "Alkaline (0.9V/cell)\n"
        "Dry Cell (0.9V/cell)\n"
        "Lead Acid (1.93V/cell)\n"
        "SLA (1.93V/cell)\n"
        "AGM (1.75V/cell)\n"
        "NiCd (1.2V/cell)\n"
        "NiMH (1.2V/cell)\n"
        "Li Primary (2.0V/cell)\n"
        "Li-Ion (3.5V/cell)\n"
        "LiPO (3.5V/cell)\n"
        "LiFePO4 (2.5V/cell)");
    lv_obj_set_width(dropdown_battery_type, 260);
    lv_obj_add_event_cb(dropdown_battery_type, on_battery_type_changed, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *row_load_type = create_config_row(list, "Load Type");
    dropdown_load_type = lv_dropdown_create(row_load_type);
    lv_dropdown_set_options(dropdown_load_type, "Constant Current\nConstant Power\nConstant Impedance\nPulsed");
    lv_obj_set_width(dropdown_load_type, 260);
    lv_obj_add_event_cb(dropdown_load_type, on_load_type_changed, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *row_cells = create_config_row(list, "Series Cells");
    dropdown_series_cells = lv_dropdown_create(row_cells);
    lv_dropdown_set_options(dropdown_series_cells,
        "1S\n2S\n3S\n4S\n5S\n6S\n7S\n8S\n9S\n10S\n"
        "11S\n12S\n13S\n14S\n15S\n16S\n17S\n18S\n19S\n20S\n"
        "21S\n22S\n23S\n24S\n25S\n26S\n27S\n28S\n29S\n30S");
    lv_obj_set_width(dropdown_series_cells, 260);
    lv_obj_add_event_cb(dropdown_series_cells, on_series_cells_changed, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *row_sample = create_config_row(list, "Sample Interval");
    lv_obj_set_flex_align(row_sample, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row_sample, 8, LV_PART_MAIN);
    lv_obj_t * sample_controls = lv_obj_create(row_sample);
    lv_obj_remove_style_all(sample_controls);
    lv_obj_set_width(sample_controls, 196);
    lv_obj_set_flex_flow(sample_controls, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(sample_controls, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(sample_controls, 8, LV_PART_MAIN);
    lv_obj_t *sample_minus = create_small_button(sample_controls, "-");
    lv_obj_add_event_cb(sample_minus, on_sample_minus, LV_EVENT_CLICKED, NULL);
    value_sample_interval = lv_label_create(sample_controls);
    lv_obj_set_width(value_sample_interval, 72);
    lv_obj_add_style(value_sample_interval, &style_metric_value, LV_PART_MAIN);
    lv_obj_t *sample_plus = create_small_button(sample_controls, "+");
    lv_obj_add_event_cb(sample_plus, on_sample_plus, LV_EVENT_CLICKED, NULL);

    lv_obj_t *row_overtemp = create_config_row(list, "Overtemp Cutoff");
    lv_obj_t *overtemp_controls = lv_obj_create(row_overtemp);
    lv_obj_remove_style_all(overtemp_controls);
    lv_obj_set_width(overtemp_controls, 196);
    lv_obj_set_flex_flow(overtemp_controls, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(overtemp_controls, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(overtemp_controls, 8, LV_PART_MAIN);
    lv_obj_t *overtemp_minus = create_small_button(overtemp_controls, "-");
    lv_obj_add_event_cb(overtemp_minus, on_overtemp_minus, LV_EVENT_CLICKED, NULL);
    value_overtemp_cutoff = lv_label_create(overtemp_controls);
    lv_obj_set_width(value_overtemp_cutoff, 72);
    lv_obj_add_style(value_overtemp_cutoff, &style_metric_value, LV_PART_MAIN);
    lv_obj_t *overtemp_plus = create_small_button(overtemp_controls, "+");
    lv_obj_add_event_cb(overtemp_plus, on_overtemp_plus, LV_EVENT_CLICKED, NULL);

    lv_obj_t *row_ampacity = create_config_row(list, "Battery Ampacity");
    lv_obj_t *ampacity_controls = lv_obj_create(row_ampacity);
    lv_obj_remove_style_all(ampacity_controls);
    lv_obj_set_width(ampacity_controls, 196);
    lv_obj_set_flex_flow(ampacity_controls, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ampacity_controls, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(ampacity_controls, 8, LV_PART_MAIN);
    lv_obj_t *ampacity_minus = create_small_button(ampacity_controls, "-");
    lv_obj_add_event_cb(ampacity_minus, on_ampacity_minus, LV_EVENT_CLICKED, NULL);
    value_battery_ampacity = lv_label_create(ampacity_controls);
    lv_obj_set_width(value_battery_ampacity, 72);
    lv_obj_add_style(value_battery_ampacity, &style_metric_value, LV_PART_MAIN);
    lv_obj_t *ampacity_plus = create_small_button(ampacity_controls, "+");
    lv_obj_add_event_cb(ampacity_plus, on_ampacity_plus, LV_EVENT_CLICKED, NULL);

    lv_obj_t *row_pack_cutoff = create_config_row(list, "Pack Cutoff");
    lv_obj_set_flex_align(row_pack_cutoff, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row_pack_cutoff, 10, LV_PART_MAIN);
    value_pack_cutoff_preview = lv_label_create(row_pack_cutoff);
    lv_obj_set_width(value_pack_cutoff_preview, 240);
    lv_obj_add_style(value_pack_cutoff_preview, &style_metric_value, LV_PART_MAIN);
    lv_obj_set_style_text_align(value_pack_cutoff_preview, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);

    lv_obj_t *footer = lv_obj_create(screen_config);
    lv_obj_set_size(footer, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_add_style(footer, &style_section, LV_PART_MAIN);
    lv_obj_set_flex_flow(footer, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(footer, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *back_btn = lv_btn_create(footer);
    lv_obj_set_size(back_btn, 200, 64);
    lv_obj_add_style(back_btn, &style_nav_button, LV_PART_MAIN);
    lv_obj_add_event_cb(back_btn, on_back_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Back");
    lv_obj_add_style(back_label, &style_button_text, LV_PART_MAIN);
    lv_obj_center(back_label);

    lv_obj_t *apply_btn = lv_btn_create(footer);
    lv_obj_set_size(apply_btn, 220, 64);
    lv_obj_add_style(apply_btn, &style_nav_button_active, LV_PART_MAIN);
    lv_obj_add_event_cb(apply_btn, on_apply_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *apply_label = lv_label_create(apply_btn);
    lv_label_set_text(apply_label, "Apply");
    lv_obj_add_style(apply_label, &style_button_text, LV_PART_MAIN);
    lv_obj_center(apply_label);

    pending_config = active_config;
    refresh_config_values();
}

void ui_init(void)
{
    init_styles();
    build_monitor_screen();
    build_config_screen();
    lv_disp_load_scr(screen_monitor);
}

void ui_set_channel_data(uint8_t channel, const ui_channel_data_t *data)
{
    if (channel != 0 || data == NULL) {
        return;
    }

    latest_data = *data;
    latest_data_valid = true;
    apply_values(data);

    history_sample_counter++;
    if ((history_sample_counter % history_stride) != 0) {
        refresh_chart();
        return;
    }

    if (history_count >= HISTORY_MAX) {
        compress_history();
    }

    if (history_count < HISTORY_MAX) {
        uint16_t idx = history_count;
        history_raw[METRIC_VOLTAGE][idx] = data->voltage_v;
        history_raw[METRIC_CURRENT][idx] = data->current_ma;
        history_raw[METRIC_POWER][idx] = data->power_w;
        history_raw[METRIC_ENERGY][idx] = data->energy_wh;
        if (isnan(data->load_temp_f)) {
            history_raw[METRIC_LOAD_TEMP][idx] = (idx > 0) ? history_raw[METRIC_LOAD_TEMP][idx - 1] : 0.0f;
        } else {
            history_raw[METRIC_LOAD_TEMP][idx] = data->load_temp_f;
        }
        history_count++;
    }

    refresh_chart();
}

void ui_set_sensor_connected(bool connected)
{
    if (sensor_status_label != NULL) {
        lv_label_set_text(sensor_status_label, connected ? "Active INA226: Connected" : "Active INA226: Not Connected");
    }
}

void ui_set_sensor_status(const char *status_text)
{
    if (sensor_status_label != NULL && status_text != NULL) {
        lv_label_set_text(sensor_status_label, status_text);
    }
}

bool ui_consume_reset_request(uint8_t channel)
{
    if (channel != 0) {
        return false;
    }

    bool requested = reset_requested;
    reset_requested = false;
    return requested;
}

bool ui_consume_start_request(uint8_t channel)
{
    if (channel != 0) {
        return false;
    }

    bool requested = start_requested;
    start_requested = false;
    return requested;
}

bool ui_consume_stop_request(uint8_t channel)
{
    if (channel != 0) {
        return false;
    }

    bool requested = stop_requested;
    stop_requested = false;
    return requested;
}

void ui_set_test_running(bool running)
{
    if (start_button_label != NULL) {
        lv_label_set_text(start_button_label, running ? "Testing..." : "Start Test");
    }

    if (start_button_obj != NULL) {
        if (running) {
            lv_obj_add_state(start_button_obj, LV_STATE_DISABLED);
        } else {
            lv_obj_clear_state(start_button_obj, LV_STATE_DISABLED);
        }
    }

    if (stop_button_obj != NULL) {
        if (running) {
            lv_obj_clear_state(stop_button_obj, LV_STATE_DISABLED);
        } else {
            lv_obj_add_state(stop_button_obj, LV_STATE_DISABLED);
        }
    }
}

void ui_request_reset(uint8_t channel)
{
    if (channel != 0) {
        return;
    }

    reset_requested = true;
    clear_history();
}

void ui_load_channel_screen(uint8_t channel)
{
    (void)channel;
    if (screen_monitor != NULL) {
        lv_disp_load_scr(screen_monitor);
    }
}

void ui_set_config(const ui_config_t *config)
{
    if (config == NULL) {
        return;
    }

    active_config = *config;
    pending_config = *config;
    refresh_config_values();
    apply_chart_visibility();

    if (latest_data_valid) {
        apply_values(&latest_data);
    }
}

bool ui_consume_config_update(ui_config_t *config)
{
    if (!config_update_pending || config == NULL) {
        return false;
    }

    *config = active_config;
    config_update_pending = false;
    return true;
}
