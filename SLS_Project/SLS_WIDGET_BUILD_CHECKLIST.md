# SquareLine Widget Build Checklist (Step-by-Step)

Use this as a build order inside SquareLine Studio so the recreated UI matches `src/ui/ui.c` behavior and is easy to wire back into firmware.

## 0) Project setup

- Open `SLS_Project/Tab5_INA3221_Energy_Monitor.spj`
- Confirm in Project Settings:
  - Resolution: `1280x720`
  - LVGL: `8.3.6`
  - Dark theme enabled
  - UI export path: `.../src/ui`

## 1) Create screens

Create 3 screens (exact names):

1. `ui_ScreenCH1`
2. `ui_ScreenCH2`
3. `ui_ScreenCH3`

Each screen uses same layout; only channel title and active nav style differ.

## 2) Global style tokens (create once)

Create reusable styles with these names:

- `st_screen`
- `st_header`
- `st_section`
- `st_channel_title`
- `st_metric_label`
- `st_metric_value`
- `st_btn_reset`
- `st_btn_text`
- `st_btn_nav`
- `st_btn_nav_active`

Suggested values (from runtime UI):

- `st_screen`: bg `#0B1020`, pad all `12`
- `st_header`: text `#EAF1FF`, Montserrat 24, centered
- `st_section`: bg `#151D35`, border `#2E3C66` width `2`, radius `12`, pad `8`
- `st_channel_title`: text `#F7FAFF`, Montserrat 32, centered
- `st_metric_label`: text `#B8C6EC`, Montserrat 24
- `st_metric_value`: text `#FFFFFF`, Montserrat 28
- `st_btn_reset`: red background, radius `10`
- `st_btn_text`: Montserrat 20
- `st_btn_nav`: bg `#26375E`, radius `9`
- `st_btn_nav_active`: bg `#3A5EA8`

## 3) Build one screen template (CH1)

On `ui_ScreenCH1`, create this object tree (top to bottom):

- `cont_root_ch1` (full screen, column flex, row gap `6`, style `st_screen`)
  - `lbl_header_ch1` (text: `CrazyUncleBurton.com Energy Monitor`, width `100%`, style `st_header`)
  - `card_chart_ch1` (width `100%`, height ~`38%`, style `st_section`, column)
    - `chart_ch1` (line chart, width `100%`, height ~`82%`)
    - `row_reset_ch1` (row, centered)
      - `btn_reset_ch1` (width ~`88%`, height `74`, style `st_btn_reset`)
        - `lbl_reset_ch1` (text: `Reset Channel`, style `st_btn_text`)
  - `card_metrics_ch1` (width `100%`, content height, style `st_section`, column)
    - `lbl_title_ch1` (text: `Channel 1`, style `st_channel_title`)
    - `row_voltage_ch1` (row)
      - `lbl_voltage_name_ch1` (text `Voltage`, red, style `st_metric_label`)
      - `lbl_voltage_colon_ch1` (text `:`)
      - `lbl_voltage_value_ch1` (text `0.000 V`, red, style `st_metric_value`)
    - `row_current_ch1`
      - `lbl_current_name_ch1` (text `Current`, yellow)
      - `lbl_current_colon_ch1` (text `:`)
      - `lbl_current_value_ch1` (text `0.00 mA`, yellow)
    - `row_power_ch1`
      - `lbl_power_name_ch1` (text `Power`, green)
      - `lbl_power_colon_ch1` (text `:`)
      - `lbl_power_value_ch1` (text `0.000 W`, green)
    - `row_energy_ch1`
      - `lbl_energy_name_ch1` (text `Total Energy`, blue)
      - `lbl_energy_colon_ch1` (text `:`)
      - `lbl_energy_value_ch1` (text `0.0000 Wh`, blue)
  - `card_nav_ch1` (width `100%`, content height, style `st_section`)
    - `row_nav_ch1` (row, space-evenly)
      - `btn_nav_ch1_to_ch1` (text `CH1`, style `st_btn_nav` + `st_btn_nav_active`)
      - `btn_nav_ch1_to_ch2` (text `CH2`, style `st_btn_nav`)
      - `btn_nav_ch1_to_ch3` (text `CH3`, style `st_btn_nav`)

## 4) Duplicate for CH2 and CH3

Duplicate `ui_ScreenCH1` twice, rename all objects by suffix:

- `_ch1` -> `_ch2` for `ui_ScreenCH2`
- `_ch1` -> `_ch3` for `ui_ScreenCH3`

Then change:

- `lbl_title_ch2` text -> `Channel 2`
- `lbl_title_ch3` text -> `Channel 3`
- Active nav style:
  - CH2 screen: active only on `CH2`
  - CH3 screen: active only on `CH3`

## 5) Chart setup per screen

For each chart (`chart_ch1/ch2/ch3`):

- Type: line
- Series count: 4
  - Voltage: red
  - Current: yellow
  - Power: green
  - Energy: blue
- Y range: `-1000` to `1000`
- Div lines: `9x8`
- Chart bg: `#0F1528`
- Border: `#32456F`, width `2`

## 6) Navigation events

Set button click events:

- On CH1 screen:
  - `btn_nav_ch1_to_ch1` -> load `ui_ScreenCH1`
  - `btn_nav_ch1_to_ch2` -> load `ui_ScreenCH2`
  - `btn_nav_ch1_to_ch3` -> load `ui_ScreenCH3`
- On CH2 screen:
  - `btn_nav_ch2_to_ch1` -> load `ui_ScreenCH1`
  - `btn_nav_ch2_to_ch2` -> load `ui_ScreenCH2`
  - `btn_nav_ch2_to_ch3` -> load `ui_ScreenCH3`
- On CH3 screen:
  - `btn_nav_ch3_to_ch1` -> load `ui_ScreenCH1`
  - `btn_nav_ch3_to_ch2` -> load `ui_ScreenCH2`
  - `btn_nav_ch3_to_ch3` -> load `ui_ScreenCH3`

## 7) Reset button events

For now wire click events to named callbacks (stub names):

- `btn_reset_ch1` -> `on_reset_ch1`
- `btn_reset_ch2` -> `on_reset_ch2`
- `btn_reset_ch3` -> `on_reset_ch3`

Firmware can map these callbacks to the existing reset-request mechanism currently in `src/ui/ui.c`.

## 8) Export + integration checklist

- Export UI code to `src/ui`
- Copy `SLS_Project/ui_events_glue_template.h` and `SLS_Project/ui_events_glue_template.c` into `src/ui` (or merge into exported events files)
- In SquareLine event bindings, point reset/nav callbacks to:
  - `on_reset_ch1`, `on_reset_ch2`, `on_reset_ch3`
  - `on_nav_to_ch1`, `on_nav_to_ch2`, `on_nav_to_ch3`
- Build once in PlatformIO
- Verify:
  - All 3 screens load
  - Nav buttons switch screens
  - Reset buttons trigger callbacks
  - Label objects are accessible for runtime data updates

## 9) Runtime symbol mapping guide

When integrating exported SLS code with runtime updates, map by object purpose:

- Voltage labels: `lbl_voltage_value_ch1/ch2/ch3`
- Current labels: `lbl_current_value_ch1/ch2/ch3`
- Power labels: `lbl_power_value_ch1/ch2/ch3`
- Energy labels: `lbl_energy_value_ch1/ch2/ch3`
- Charts: `chart_ch1/ch2/ch3`

This mirrors the existing runtime arrays in `src/ui/ui.c`:

- `value_voltage[]`, `value_current[]`, `value_power[]`, `value_energy[]`
- `chart_obj[]`

## 10) Keep in firmware (do not expect SLS to generate)

- History compression/stride logic
- Dynamic chart normalization
- Data formatting precision per metric
- `ui_set_channel_data()` and `ui_consume_reset_request()` semantics
