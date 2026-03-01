/*

  M5Stack Tab5 and TI INA3221 Current Sensor

  by Bryan A. "CrazyUncleBurton" Thompson

  Last Updated 2/28/2026


  Dependencies:

  ESP-Arduino >= V3.2.54 (tested also working with 3.3.0-alpha1)
  M5Unified = 0.2.13
  M5GFX = V0.2.19
  LVGL = V8.4.0
  Adafruit_INA3221 = V1.0.1

  lv_conf.h:
  #define LV_COLOR_DEPTH 16
  #define LV_COLOR_16_SWAP 1
  #define LV_MEM_CUSTOM 1
  #define LV_TICK_CUSTOM 1

  Build Options:

  Board: "ESP32P4 Dev Module"
  USB CDC on boot: "Enabled"
  Flash Size: "16MB (128Mb)"
  Board Build Partitions:  "partitions.csv"
  PSRAM: "Enabled"
  Upload Mode: "UART / Hardware CDC"
  USB Mode: "Hardware CDC and JTAG"

  Notes:
  Square Line Studio V1.5.1 project is included so that you can experiment with your own exported UI files.

*/


// #include <M5Unified.h>
#include <Wire.h>
#include "Adafruit_INA3221.h"
#include <M5GFX.h>
#include <math.h>
#include "lvgl.h"
#include "ui/ui.h"
#include "pins_config.h"


// Create an INA3221 object
Adafruit_INA3221 ina3221;


// Create an M5GFX object.
M5GFX display;


// Variables
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *buf;
uint16_t sensorAddress = 0x40;
static uint32_t lastSampleMs = 0;
static uint32_t lastDebugMs = 0;
static uint32_t lastLvTickMs = 0;
static uint32_t lastSensorRetryMs = 0;
static float energyWh[3] = {0.0f, 0.0f, 0.0f};
static ui_channel_data_t channelData[3];
static bool sensorPresent = false;


static void setSensorPresent(bool present)
{
  sensorPresent = present;
  ui_set_sensor_connected(sensorPresent);
}


static bool initializeINA3221()
{
  if (!ina3221.begin(sensorAddress, &Wire)) {
    return false;
  }

  ina3221.setAveragingMode(INA3221_AVG_16_SAMPLES);
  for (uint8_t i = 0; i < 3; i++) {
    ina3221.setShuntResistance(i, 0.05);
  }

  return true;
}


void updateINA3221AndUi()
{
  uint32_t nowMs = millis();
  uint32_t deltaMs = (lastSampleMs == 0) ? 0 : (nowMs - lastSampleMs);
  float deltaHours = (float)deltaMs / 3600000.0f;

  if (!sensorPresent) {
    for (uint8_t i = 0; i < 3; i++) {
      if (ui_consume_reset_request(i)) {
        energyWh[i] = 0.0f;
      }

      channelData[i].voltage_v = 0.0f;
      channelData[i].current_ma = 0.0f;
      channelData[i].power_w = 0.0f;
      channelData[i].energy_wh = energyWh[i];

      ui_set_channel_data(i, &channelData[i]);
    }

    lastSampleMs = nowMs;
    return;
  }

  for (uint8_t i = 0; i < 3; i++) {
    if (ui_consume_reset_request(i)) {
      energyWh[i] = 0.0f;
    }

    float voltageV = ina3221.getBusVoltage(i);
    float currentA = ina3221.getCurrentAmps(i);

    if (isnan(voltageV) || isnan(currentA)) {
      setSensorPresent(false);
      Serial.println("INA3221 read failed, sensor unavailable.");
      updateINA3221AndUi();
      return;
    }

    float powerW = voltageV * currentA;

    if (deltaMs > 0) {
      energyWh[i] += powerW * deltaHours;
    }

    channelData[i].voltage_v = voltageV;
    channelData[i].current_ma = currentA * 1000.0f;
    channelData[i].power_w = powerW;
    channelData[i].energy_wh = energyWh[i];

    ui_set_channel_data(i, &channelData[i]);
  }

  lastSampleMs = nowMs;
}


void lv_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    display.pushImageDMA(area->x1, area->y1, w, h, (uint16_t *)&color_p->full); 
    lv_disp_flush_ready(disp);
}


void my_rounder(lv_disp_drv_t *disp_drv, lv_area_t *area)
{
    if (area->x1 % 2 != 0)
        area->x1 += 1;
    if (area->y1 % 2 != 0)
        area->y1 += 1;

    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    if (w % 2 != 0)
        area->x2 -= 1;
    if (h % 2 != 0)
        area->y2 -= 1;
}


static void lv_indev_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data)
{  
     lgfx::touch_point_t tp[3];
  uint8_t touchpad = display.getTouch(tp, 3);
       if (touchpad > 0)
       {
          data->state = LV_INDEV_STATE_PR;
          data->point.x = tp[0].x;
          data->point.y = tp[0].y;
          //Serial.printf("X: %d   Y: %d\n", tp[0].x, tp[0].y); //for testing
       }
       else
       {
        data->state = LV_INDEV_STATE_REL;
       }

      data->continue_reading = false;
}

void setup()
{

   /*Initialize Tab5 MIPI-DSI display*/
    display.init();

    Serial.begin(115200);//for debug

    /*Initialize LVGL*/
    lv_init();
    buf = (lv_color_t *)heap_caps_malloc(sizeof(lv_color_t) * LVGL_LCD_BUF_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, LVGL_LCD_BUF_SIZE);
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = EXAMPLE_LCD_H_RES;
    disp_drv.ver_res = EXAMPLE_LCD_V_RES;
    //disp_drv.rounder_cb = my_rounder; 
    disp_drv.flush_cb = lv_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    disp_drv.sw_rotate = 0;
    lv_disp_drv_register(&disp_drv);

    /*Initialize touch*/
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = lv_indev_read;
    lv_indev_drv_register(&indev_drv);     

    /*Start UI*/
    ui_init();   
    display.setBrightness(255);  

    /* Start I2C */
    Wire.begin(I2C_SDA, I2C_SCL);
    delay(1000); 
    
    /* Initialize the INA3221 */
    setSensorPresent(initializeINA3221());
    if (!sensorPresent) {
      Serial.println("INA3221 not found at startup, running without sensor data.");
    } else {
      Serial.println("INA3221 initialized.");
    }

    updateINA3221AndUi();

}


void loop()
{
  uint32_t now = millis();
  if (lastLvTickMs == 0) {
    lastLvTickMs = now;
  }
  uint32_t elapsed = now - lastLvTickMs;
  if (elapsed > 0) {
    lv_tick_inc(elapsed);
    lastLvTickMs = now;
  }

  lv_timer_handler();

  if (!sensorPresent && (now - lastSensorRetryMs >= 2000)) {
    lastSensorRetryMs = now;
    setSensorPresent(initializeINA3221());
    if (sensorPresent) {
      Serial.println("INA3221 detected and reinitialized.");
      lastSampleMs = 0;
    } else {
      Serial.println("INA3221 still not detected.");
    }
  }

  if (now - lastSampleMs >= 200) {
    updateINA3221AndUi();
  }

  if (now - lastDebugMs >= 1000) {
    Serial.printf(
      "CH1: %.3fV %.2fmA %.3fW %.6fWh | CH2: %.3fV %.2fmA %.3fW %.6fWh | CH3: %.3fV %.2fmA %.3fW %.6fWh\n",
      channelData[0].voltage_v, channelData[0].current_ma, channelData[0].power_w, channelData[0].energy_wh,
      channelData[1].voltage_v, channelData[1].current_ma, channelData[1].power_w, channelData[1].energy_wh,
      channelData[2].voltage_v, channelData[2].current_ma, channelData[2].power_w, channelData[2].energy_wh
    );
    lastDebugMs = now;
  }

  delay(5);

}
