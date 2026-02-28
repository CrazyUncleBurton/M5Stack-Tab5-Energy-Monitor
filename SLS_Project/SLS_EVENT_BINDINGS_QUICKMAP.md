# SquareLine Event Bindings Quick Map

Use this after your 3-screen widget tree is created in SquareLine.

## Required callback names (already implemented in src/ui/ui_events.c)

- `on_reset_ch1`
- `on_reset_ch2`
- `on_reset_ch3`
- `on_nav_to_ch1`
- `on_nav_to_ch2`
- `on_nav_to_ch3`

## Bindings by widget

### Screen CH1

- `btn_reset_ch1` -> `CLICKED` -> `CALL FUNCTION` -> `on_reset_ch1`
- `btn_nav_ch1_to_ch1` -> `CLICKED` -> `CALL FUNCTION` -> `on_nav_to_ch1`
- `btn_nav_ch1_to_ch2` -> `CLICKED` -> `CALL FUNCTION` -> `on_nav_to_ch2`
- `btn_nav_ch1_to_ch3` -> `CLICKED` -> `CALL FUNCTION` -> `on_nav_to_ch3`

### Screen CH2

- `btn_reset_ch2` -> `CLICKED` -> `CALL FUNCTION` -> `on_reset_ch2`
- `btn_nav_ch2_to_ch1` -> `CLICKED` -> `CALL FUNCTION` -> `on_nav_to_ch1`
- `btn_nav_ch2_to_ch2` -> `CLICKED` -> `CALL FUNCTION` -> `on_nav_to_ch2`
- `btn_nav_ch2_to_ch3` -> `CLICKED` -> `CALL FUNCTION` -> `on_nav_to_ch3`

### Screen CH3

- `btn_reset_ch3` -> `CLICKED` -> `CALL FUNCTION` -> `on_reset_ch3`
- `btn_nav_ch3_to_ch1` -> `CLICKED` -> `CALL FUNCTION` -> `on_nav_to_ch1`
- `btn_nav_ch3_to_ch2` -> `CLICKED` -> `CALL FUNCTION` -> `on_nav_to_ch2`
- `btn_nav_ch3_to_ch3` -> `CLICKED` -> `CALL FUNCTION` -> `on_nav_to_ch3`

## Include hints in exported code

In your exported `src/ui` module:

- Add `#include "ui_events.h"` where SquareLine expects callback prototypes.
- Ensure `ui_events.c` is present (already provided in this repo at `src/ui/ui_events.c`).

## Why this cannot be fully auto-wired right now

The current `SLS_Project/Tab5_INA3221_Energy_Monitor.spj` is a reconstructed legacy-format base and does not yet contain the new CH1/CH2/CH3 widget objects. Once those objects exist, this quick map is a direct click-through setup.
