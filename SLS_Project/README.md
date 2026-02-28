# SquareLine Studio Project Sync

This folder tracks the SquareLine workflow for the **current code-first UI** used by this project.

## Reconstruction status

- Reconstructed now: a **full editable SquareLine project base** (`.spj/.sll/.slt`) and assets copied from the last working legacy SLS project format.
- Retargeted now: default export paths point to this repo (`src/ui`) and this folder (`SLS_Project`).
- Still manual: rebuilding the new 3-channel energy monitor widget tree in SquareLine (SquareLine cannot reverse-import runtime C automatically).

## Current source of truth

- Runtime UI implementation: `src/ui/ui.c`, `src/ui/ui.h`
- Runtime snapshot copy for SLS reference: `SLS_Project/src_ui_snapshot/`

## Important limitation

SquareLine Studio cannot reverse-import the runtime/generated C files into editable object trees automatically.

To get an editable SquareLine project that matches this UI, rebuild it in SquareLine using:

- `SLS_Project/UI_RECONSTRUCTION_SPEC.md`
- `SLS_Project/SLS_WIDGET_BUILD_CHECKLIST.md`
- `SLS_Project/ui_events_glue_template.h`
- `SLS_Project/ui_events_glue_template.c`
- `SLS_Project/SLS_EVENT_BINDINGS_QUICKMAP.md`

## Recommended workflow

1. Open SquareLine Studio (`1.5.1`) and create a `1280x720` LVGL `8.3.6` landscape project.
2. Recreate the 3-channel screens using the reconstruction spec.
3. Export generated UI code to `src/ui`.
4. Keep firmware logic in `src/main.cpp` and any runtime-only chart logic needed by the project.

## Metadata files in this folder

- `Tab5_INA3221_Energy_Monitor.spj` (sync metadata)
- `Tab5_INA3221_Energy_Monitor.sll` (sync/exports tracking)
- `Themes.slt` (theme values)

## Legacy backup

The previous full SquareLine project and generated UI are preserved at:

- `backup/20260228_pre-redesign/SLS_Project_legacy/`
