// Pin assignments for M5Stack Tab5

#pragma once

/*********************** M5Stack Tab5 config *************************/

// Display resolution (Tab5 MIPI-DSI panel)
#define EXAMPLE_LCD_H_RES 720
#define EXAMPLE_LCD_V_RES 1280

// I2C (external header) - used by INA3221 in this project
#define I2C_SDA_EXT 53
#define I2C_SCL_EXT 54
// Default I2C used by the project (matches main.cpp)
#define I2C_SDA I2C_SDA_EXT
#define I2C_SCL I2C_SCL_EXT

// SPI pins for display (Tab5, per M5Unified mapping)
#define TFT_SPI_SCLK 43
#define TFT_SPI_MOSI 44
#define TFT_SPI_MISO 39
#define TFT_SPI_CS   42

// LVGL buffer sizes
#define LVGL_LCD_BUF_SIZE     (EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES)
#define SEND_BUF_SIZE         (EXAMPLE_LCD_H_RES * 10)

// Other commented device pins (keep for reference)
// #define BATTERY_VOLTAGE_ADC_DATA -1
// #define SD_CS -1
// #define PCF8563_INT -1
// #define IMU_INT -1
// #define I2S_BCK_IO 9

// #define I2S_DI_IO 10

// #define I2S_WS_IO 45

// #define I2S_DO_IO 8



// #define MCLKPIN             16

// #define BCLKPIN              9

