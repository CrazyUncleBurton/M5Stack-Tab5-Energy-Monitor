#include "ui_events_glue_template.h"

/*
  Copy this file into your exported src/ui folder (or merge into ui_events.c)
  after SquareLine export, then wire these callback names to your buttons:

  - Reset CH1/CH2/CH3 -> on_reset_ch1/on_reset_ch2/on_reset_ch3
  - Nav CH1/CH2/CH3   -> on_nav_to_ch1/on_nav_to_ch2/on_nav_to_ch3

  It depends on the runtime UI API in src/ui/ui.h:
  - ui_request_reset(channel)
  - ui_load_channel_screen(channel)
*/

#include "ui.h"

static void ignore_event_arg(lv_event_t *e)
{
    (void)e;
}

void on_reset_ch1(lv_event_t *e)
{
    ignore_event_arg(e);
    ui_request_reset(0);
}

void on_reset_ch2(lv_event_t *e)
{
    ignore_event_arg(e);
    ui_request_reset(1);
}

void on_reset_ch3(lv_event_t *e)
{
    ignore_event_arg(e);
    ui_request_reset(2);
}

void on_nav_to_ch1(lv_event_t *e)
{
    ignore_event_arg(e);
    ui_load_channel_screen(0);
}

void on_nav_to_ch2(lv_event_t *e)
{
    ignore_event_arg(e);
    ui_load_channel_screen(1);
}

void on_nav_to_ch3(lv_event_t *e)
{
    ignore_event_arg(e);
    ui_load_channel_screen(2);
}
