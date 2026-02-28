#include "ui_events.h"

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
