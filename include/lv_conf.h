/* Minimal lv_conf.h for PlatformIO build
   Copy and expand from lvgl's lv_conf_template.h when needed.
*/
#ifndef LV_CONF_H
#define LV_CONF_H

/* Color depth and swap recommended for Tab5 */
#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 1

/* Use LVGL default memory implementation */
#define LV_MEM_CUSTOM 0

/* Use LVGL default tick handling */
#define LV_TICK_CUSTOM 0

/* Enable larger built-in fonts for portrait dashboard readability */
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_18 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_32 1

#endif /* LV_CONF_H */
