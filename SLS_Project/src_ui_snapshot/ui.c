#include "ui.h"

#include <stdio.h>
#include <string.h>

#define CHANNEL_COUNT 3
#define METRIC_COUNT 4
#define HISTORY_MAX 900

typedef enum {
    METRIC_VOLTAGE = 0,
    METRIC_CURRENT = 1,
    METRIC_POWER = 2,
    METRIC_ENERGY = 3
} metric_t;

static lv_obj_t *screen_channel[CHANNEL_COUNT];
static lv_obj_t *value_voltage[CHANNEL_COUNT];
static lv_obj_t *value_current[CHANNEL_COUNT];
static lv_obj_t *value_power[CHANNEL_COUNT];
static lv_obj_t *value_energy[CHANNEL_COUNT];
static lv_obj_t *chart_obj[CHANNEL_COUNT];
static lv_chart_series_t *chart_series[CHANNEL_COUNT][METRIC_COUNT];

static bool reset_requested[CHANNEL_COUNT] = {false, false, false};
static uint16_t history_count[CHANNEL_COUNT] = {0, 0, 0};
static uint32_t history_sample_counter[CHANNEL_COUNT] = {0, 0, 0};
static uint16_t history_stride[CHANNEL_COUNT] = {1, 1, 1};
static float history_raw[CHANNEL_COUNT][METRIC_COUNT][HISTORY_MAX];
static lv_coord_t history_chart[CHANNEL_COUNT][METRIC_COUNT][HISTORY_MAX];
static ui_channel_data_t latest_data[CHANNEL_COUNT];
static bool latest_data_valid[CHANNEL_COUNT] = {false, false, false};

static lv_style_t style_screen;
static lv_style_t style_header;
static lv_style_t style_section;
static lv_style_t style_channel_title;
static lv_style_t style_metric_label;
static lv_style_t style_metric_value;
static lv_style_t style_button;
static lv_style_t style_button_text;
static lv_style_t style_nav_button;
static lv_style_t style_nav_button_active;

static void set_value_text(lv_obj_t *label, float value, uint8_t decimals, const char *unit);

static void apply_channel_values(uint8_t channel, const ui_channel_data_t *data)
{
    if (channel >= CHANNEL_COUNT || data == NULL) {
        return;
    }

    set_value_text(value_voltage[channel], data->voltage_v, 3, "V");
    set_value_text(value_current[channel], data->current_ma, 2, "mA");
    set_value_text(value_power[channel], data->power_w, 3, "W");
    set_value_text(value_energy[channel], data->energy_wh, 4, "Wh");
}

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

static void refresh_chart(uint8_t channel)
{
    if (channel >= CHANNEL_COUNT || chart_obj[channel] == NULL) {
        return;
    }

    uint16_t count = history_count[channel];
    if (count == 0) {
        lv_chart_set_point_count(chart_obj[channel], 1);
        for (uint8_t metric = 0; metric < METRIC_COUNT; metric++) {
            history_chart[channel][metric][0] = 0;
        }
        lv_chart_set_range(chart_obj[channel], LV_CHART_AXIS_PRIMARY_Y, 0, 1);
        lv_chart_refresh(chart_obj[channel]);
        return;
    }

    lv_chart_set_point_count(chart_obj[channel], count);
    lv_chart_set_div_line_count(chart_obj[channel], 9, 8);

    for (uint8_t metric = 0; metric < METRIC_COUNT; metric++) {
        float minv = history_raw[channel][metric][0];
        float maxv = history_raw[channel][metric][0];

        for (uint16_t i = 1; i < count; i++) {
            float v = history_raw[channel][metric][i];
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

        for (uint16_t i = 0; i < count; i++) {
            float normalized = (history_raw[channel][metric][i] - center) / half_span;
            if (normalized > 1.2f) {
                normalized = 1.2f;
            } else if (normalized < -1.2f) {
                normalized = -1.2f;
            }
            history_chart[channel][metric][i] = (lv_coord_t)(normalized * 900.0f);
        }
    }

    lv_chart_set_range(chart_obj[channel], LV_CHART_AXIS_PRIMARY_Y, -1000, 1000);
    lv_chart_set_x_start_point(chart_obj[channel], chart_series[channel][METRIC_VOLTAGE], 0);
    lv_chart_set_x_start_point(chart_obj[channel], chart_series[channel][METRIC_CURRENT], 0);
    lv_chart_set_x_start_point(chart_obj[channel], chart_series[channel][METRIC_POWER], 0);
    lv_chart_set_x_start_point(chart_obj[channel], chart_series[channel][METRIC_ENERGY], 0);
    lv_chart_refresh(chart_obj[channel]);
}

static void clear_channel_history(uint8_t channel)
{
    if (channel >= CHANNEL_COUNT) {
        return;
    }

    history_count[channel] = 0;
    history_sample_counter[channel] = 0;
    history_stride[channel] = 1;
    for (uint8_t metric = 0; metric < METRIC_COUNT; metric++) {
        memset(history_raw[channel][metric], 0, sizeof(history_raw[channel][metric]));
        memset(history_chart[channel][metric], 0, sizeof(history_chart[channel][metric]));
    }
    refresh_chart(channel);
}

static void compress_channel_history(uint8_t channel)
{
    uint16_t count = history_count[channel];
    if (count < 2) {
        return;
    }

    for (uint8_t metric = 0; metric < METRIC_COUNT; metric++) {
        uint16_t dst = 0;
        for (uint16_t src = 0; src < count; src += 2) {
            float r0 = history_raw[channel][metric][src];
            float r1 = (src + 1 < count) ? history_raw[channel][metric][src + 1] : r0;
            history_raw[channel][metric][dst] = (r0 + r1) * 0.5f;

            lv_coord_t v0 = history_chart[channel][metric][src];
            lv_coord_t v1 = (src + 1 < count) ? history_chart[channel][metric][src + 1] : v0;
            history_chart[channel][metric][dst++] = (lv_coord_t)((v0 + v1) / 2);
        }
    }

    history_count[channel] = (uint16_t)((count + 1) / 2);
    history_stride[channel] = (uint16_t)(history_stride[channel] * 2);
}

static void on_reset_clicked(lv_event_t *e)
{
    uint8_t channel = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if (channel >= CHANNEL_COUNT) {
        return;
    }

    reset_requested[channel] = true;
    clear_channel_history(channel);
}

static void on_nav_clicked(lv_event_t *e)
{
    uint8_t channel = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if (channel < CHANNEL_COUNT && screen_channel[channel] != NULL) {
        lv_disp_load_scr(screen_channel[channel]);
    }
}

static void on_screen_loaded(lv_event_t *e)
{
    uint8_t channel = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if (channel >= CHANNEL_COUNT) {
        return;
    }

    if (latest_data_valid[channel]) {
        apply_channel_values(channel, &latest_data[channel]);
    }
    refresh_chart(channel);
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
    lv_obj_set_width(label_name, lv_pct(38));
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
    lv_obj_set_width(*value_label, lv_pct(56));
    lv_obj_add_style(*value_label, &style_metric_value, LV_PART_MAIN);
    lv_obj_set_style_text_color(*value_label, color, LV_PART_MAIN);
    lv_obj_set_style_text_align(*value_label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);

    return row;
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
    lv_style_set_text_font(&style_metric_label, &lv_font_montserrat_24);

    lv_style_init(&style_metric_value);
    lv_style_set_text_color(&style_metric_value, lv_color_hex(0xFFFFFF));
    lv_style_set_text_font(&style_metric_value, &lv_font_montserrat_28);

    lv_style_init(&style_button);
    lv_style_set_radius(&style_button, 10);
    lv_style_set_bg_color(&style_button, lv_palette_main(LV_PALETTE_RED));
    lv_style_set_bg_opa(&style_button, LV_OPA_COVER);
    lv_style_set_text_color(&style_button, lv_color_white());
    lv_style_set_pad_ver(&style_button, 12);
    lv_style_set_pad_hor(&style_button, 16);

    lv_style_init(&style_button_text);
    lv_style_set_text_font(&style_button_text, &lv_font_montserrat_20);

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

static void build_channel_screen(uint8_t channel)
{
    screen_channel[channel] = lv_obj_create(NULL);
    lv_obj_remove_style_all(screen_channel[channel]);
    lv_obj_set_size(screen_channel[channel], LV_PCT(100), LV_PCT(100));
    lv_obj_add_style(screen_channel[channel], &style_screen, LV_PART_MAIN);
    lv_obj_add_event_cb(screen_channel[channel], on_screen_loaded, LV_EVENT_SCREEN_LOADED, (void *)(uintptr_t)channel);
    lv_obj_set_flex_flow(screen_channel[channel], LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(screen_channel[channel], LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(screen_channel[channel], 6, LV_PART_MAIN);

    lv_obj_t *header = lv_label_create(screen_channel[channel]);
    lv_label_set_text(header, "CrazyUncleBurton.com Energy Monitor");
    lv_obj_set_width(header, lv_pct(100));
    lv_obj_add_style(header, &style_header, LV_PART_MAIN);

    lv_obj_t *chart_card = lv_obj_create(screen_channel[channel]);
    lv_obj_set_size(chart_card, lv_pct(100), lv_pct(38));
    lv_obj_set_flex_grow(chart_card, 1);
    lv_obj_add_style(chart_card, &style_section, LV_PART_MAIN);
    lv_obj_set_flex_flow(chart_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(chart_card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(chart_card, 6, LV_PART_MAIN);

    chart_obj[channel] = lv_chart_create(chart_card);
    lv_obj_set_size(chart_obj[channel], lv_pct(100), lv_pct(82));
    lv_chart_set_type(chart_obj[channel], LV_CHART_TYPE_LINE);
    lv_chart_set_div_line_count(chart_obj[channel], 9, 8);
    lv_chart_set_range(chart_obj[channel], LV_CHART_AXIS_PRIMARY_Y, -1000, 1000);
    lv_chart_set_point_count(chart_obj[channel], 1);
    lv_obj_set_style_bg_color(chart_obj[channel], lv_color_hex(0x0F1528), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(chart_obj[channel], LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(chart_obj[channel], lv_color_hex(0x32456F), LV_PART_MAIN);
    lv_obj_set_style_border_width(chart_obj[channel], 2, LV_PART_MAIN);
    lv_obj_set_style_line_color(chart_obj[channel], lv_color_hex(0x4A5D89), LV_PART_MAIN);
    lv_obj_set_style_line_width(chart_obj[channel], 1, LV_PART_MAIN);
    lv_obj_set_style_line_opa(chart_obj[channel], LV_OPA_70, LV_PART_MAIN);
    lv_obj_set_style_pad_all(chart_obj[channel], 6, LV_PART_MAIN);
    lv_obj_set_style_line_width(chart_obj[channel], 2, LV_PART_ITEMS);

    chart_series[channel][METRIC_VOLTAGE] = lv_chart_add_series(chart_obj[channel], lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);
    chart_series[channel][METRIC_CURRENT] = lv_chart_add_series(chart_obj[channel], lv_palette_main(LV_PALETTE_YELLOW), LV_CHART_AXIS_PRIMARY_Y);
    chart_series[channel][METRIC_POWER] = lv_chart_add_series(chart_obj[channel], lv_palette_main(LV_PALETTE_GREEN), LV_CHART_AXIS_PRIMARY_Y);
    chart_series[channel][METRIC_ENERGY] = lv_chart_add_series(chart_obj[channel], lv_palette_main(LV_PALETTE_BLUE), LV_CHART_AXIS_PRIMARY_Y);

    lv_chart_set_x_start_point(chart_obj[channel], chart_series[channel][METRIC_VOLTAGE], 0);
    lv_chart_set_x_start_point(chart_obj[channel], chart_series[channel][METRIC_CURRENT], 0);
    lv_chart_set_x_start_point(chart_obj[channel], chart_series[channel][METRIC_POWER], 0);
    lv_chart_set_x_start_point(chart_obj[channel], chart_series[channel][METRIC_ENERGY], 0);

    for (uint8_t metric = 0; metric < METRIC_COUNT; metric++) {
        lv_chart_set_ext_y_array(chart_obj[channel], chart_series[channel][metric], history_chart[channel][metric]);
    }

    lv_obj_t *reset_row = lv_obj_create(chart_card);
    lv_obj_remove_style_all(reset_row);
    lv_obj_set_width(reset_row, lv_pct(100));
    lv_obj_set_height(reset_row, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(reset_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(reset_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_top(reset_row, 8, LV_PART_MAIN);

    lv_obj_t *reset_button = lv_btn_create(reset_row);
    lv_obj_set_width(reset_button, lv_pct(88));
    lv_obj_set_height(reset_button, 74);
    lv_obj_add_style(reset_button, &style_button, LV_PART_MAIN);
    lv_obj_add_event_cb(reset_button, on_reset_clicked, LV_EVENT_CLICKED, (void *)(uintptr_t)channel);
    lv_obj_set_style_translate_y(reset_button, 6, LV_PART_MAIN);
    lv_obj_t *reset_label = lv_label_create(reset_button);
    lv_label_set_text(reset_label, "Reset Channel");
    lv_obj_add_style(reset_label, &style_button_text, LV_PART_MAIN);
    lv_obj_center(reset_label);

    lv_obj_t *instant = lv_obj_create(screen_channel[channel]);
    lv_obj_set_size(instant, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_add_style(instant, &style_section, LV_PART_MAIN);
    lv_obj_set_flex_flow(instant, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(instant, 3, LV_PART_MAIN);

    char title[24];
    lv_snprintf(title, sizeof(title), "Channel %u", (unsigned)(channel + 1));
    lv_obj_t *channel_title = lv_label_create(instant);
    lv_label_set_text(channel_title, title);
    lv_obj_set_width(channel_title, lv_pct(100));
    lv_obj_add_style(channel_title, &style_channel_title, LV_PART_MAIN);

    create_metric_row(instant, "Voltage", lv_palette_main(LV_PALETTE_RED), &value_voltage[channel]);
    create_metric_row(instant, "Current", lv_palette_main(LV_PALETTE_YELLOW), &value_current[channel]);
    create_metric_row(instant, "Power", lv_palette_main(LV_PALETTE_GREEN), &value_power[channel]);
    create_metric_row(instant, "Total Energy", lv_palette_main(LV_PALETTE_BLUE), &value_energy[channel]);

    lv_obj_t *footer = lv_obj_create(screen_channel[channel]);
    lv_obj_set_size(footer, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_add_style(footer, &style_section, LV_PART_MAIN);
    lv_obj_set_flex_flow(footer, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(footer, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_column(footer, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(footer, 12, LV_PART_MAIN);

    lv_obj_t *nav_row = lv_obj_create(footer);
    lv_obj_remove_style_all(nav_row);
    lv_obj_set_size(nav_row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(nav_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(nav_row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_top(nav_row, 2, LV_PART_MAIN);

    for (uint8_t nav = 0; nav < CHANNEL_COUNT; nav++) {
        lv_obj_t *nav_btn = lv_btn_create(nav_row);
        lv_obj_add_style(nav_btn, &style_nav_button, LV_PART_MAIN);
        lv_obj_set_size(nav_btn, lv_pct(31), 74);
        if (nav == channel) {
            lv_obj_add_style(nav_btn, &style_nav_button_active, LV_PART_MAIN);
        }
        lv_obj_add_event_cb(nav_btn, on_nav_clicked, LV_EVENT_CLICKED, (void *)(uintptr_t)nav);

        char nav_text[8];
        lv_snprintf(nav_text, sizeof(nav_text), "CH%u", (unsigned)(nav + 1));
        lv_obj_t *nav_label = lv_label_create(nav_btn);
        lv_label_set_text(nav_label, nav_text);
        lv_obj_add_style(nav_label, &style_button_text, LV_PART_MAIN);
        lv_obj_center(nav_label);
    }

    refresh_chart(channel);
}

void ui_init(void)
{
    init_styles();

    for (uint8_t ch = 0; ch < CHANNEL_COUNT; ch++) {
        clear_channel_history(ch);
        build_channel_screen(ch);
    }

    lv_disp_load_scr(screen_channel[0]);
}

void ui_set_channel_data(uint8_t channel, const ui_channel_data_t *data)
{
    if (channel >= CHANNEL_COUNT || data == NULL) {
        return;
    }

    latest_data[channel] = *data;
    latest_data_valid[channel] = true;
    apply_channel_values(channel, data);

    history_sample_counter[channel]++;
    if ((history_sample_counter[channel] % history_stride[channel]) != 0) {
        refresh_chart(channel);
        return;
    }

    if (history_count[channel] >= HISTORY_MAX) {
        compress_channel_history(channel);
    }

    if (history_count[channel] < HISTORY_MAX) {
        uint16_t idx = history_count[channel];
        history_raw[channel][METRIC_VOLTAGE][idx] = data->voltage_v;
        history_raw[channel][METRIC_CURRENT][idx] = data->current_ma;
        history_raw[channel][METRIC_POWER][idx] = data->power_w;
        history_raw[channel][METRIC_ENERGY][idx] = data->energy_wh;
        history_count[channel]++;
    }

    refresh_chart(channel);
}

bool ui_consume_reset_request(uint8_t channel)
{
    if (channel >= CHANNEL_COUNT) {
        return false;
    }

    bool requested = reset_requested[channel];
    reset_requested[channel] = false;
    return requested;
}
