# UI Reconstruction Spec (SquareLine Studio)

This spec mirrors the current runtime UI implemented in:

- `src/ui/ui.c`
- `src/ui/ui.h`

Use it to rebuild an editable SquareLine project that exports back into `src/ui`.

## Project setup

- SquareLine Studio: `1.5.1`
- LVGL: `8.3.6`
- Resolution: `1280x720`
- Orientation: landscape (`rotation 0`)
- Theme: dark
- Export target: `src/ui`
- Color depth in firmware: `LV_COLOR_DEPTH 16`, `LV_COLOR_16_SWAP 1`

## Screen model

Create 3 screens:

1. `Screen_CH1`
2. `Screen_CH2`
3. `Screen_CH3`

Each screen is structurally identical except channel labels and active nav button highlight.

## Per-screen layout

Top-level container:

- Full screen
- Vertical flex layout
- Inner padding `12`
- Row gap `6`
- Background `#0B1020`

### Header label

- Text: `CrazyUncleBurton.com Energy Monitor`
- Width: `100%`
- Font: Montserrat 24
- Text color: `#EAF1FF`
- Center aligned

### Chart card

- Width: `100%`
- Height: ~`38%`
- Background `#151D35`
- Border `#2E3C66`, width `2`, radius `12`
- Padding `8`
- Column flex

Chart inside card:

- Type: line
- Width: `100%`
- Height: ~`82%`
- 4 series:
  - Voltage (red)
  - Current (yellow)
  - Power (green)
  - Energy (blue)
- Y range: `-1000..1000`
- Div lines: `9x8`
- Background `#0F1528`
- Border `#32456F`

Reset row inside card:

- Centered button
- Button text: `Reset Channel`
- Width: ~`88%`
- Height: `74`
- Radius `10`
- BG: palette red
- Text font: Montserrat 20

### Instant metrics card

- Width: `100%`
- Height: content
- Same card style as chart card
- Column layout

Title:

- `Channel 1` / `Channel 2` / `Channel 3`
- Width: `100%`
- Font: Montserrat 32
- Text color: `#F7FAFF`
- Center aligned

Rows (name, colon, value):

1. Voltage (red)
2. Current (yellow)
3. Power (green)
4. Total Energy (blue)

Typography:

- Label font: Montserrat 24
- Value font: Montserrat 28

### Footer / navigation card

- Width: `100%`
- Height: content
- Same card style
- Contains one row with 3 buttons: `CH1`, `CH2`, `CH3`
- Buttons height: `74`
- Width each: ~`31%`
- Default BG `#26375E`
- Active screen button BG `#3A5EA8`
- Font: Montserrat 20

## Event wiring

- `Reset Channel` button on CHn screen:
  - Calls reset handler for channel `n`
- `CH1/CH2/CH3` buttons:
  - Navigate to the corresponding screen

## Runtime behavior outside SquareLine

These behaviors are implemented in code and should remain in firmware-side logic:

- History buffering/compression (`HISTORY_MAX`, stride scaling)
- Dynamic chart normalization per metric
- `ui_set_channel_data()` value formatting and updates
- `ui_consume_reset_request()` request consumption

## Validation checklist

- 3 screens navigate correctly
- Active nav button highlight follows current screen
- Reset button visible and clickable on each screen
- Label/value typography and colors match runtime UI
- Exported files build in PlatformIO without LVGL config mismatch
