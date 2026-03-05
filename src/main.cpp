/*

  M5Stack Tab5 Energy Monitor

  by Bryan A. "CrazyUncleBurton" Thompson

  Last Updated 3/1/2026


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
#include "Adafruit_MCP9600.h"
#include "Adafruit_MCP9601.h"
#include "Adafruit_AD569x.h"
#include <M5GFX.h>
#include <math.h>
#include "lvgl.h"
#include "ui/ui.h"
#include "pins_config.h"


// Create an INA3221 object
Adafruit_INA3221 ina3221;
Adafruit_MCP9600 mcp9600Battery;
Adafruit_MCP9600 mcp9600Load;
Adafruit_MCP9601 mcp9601Battery;
Adafruit_MCP9601 mcp9601Load;
Adafruit_AD569x ad5693;


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
static uint32_t lastBatteryTempRetryMs = 0;
static uint32_t lastLoadTempRetryMs = 0;
static float energyWh[3] = {0.0f, 0.0f, 0.0f};
static ui_channel_data_t channelData[3];
static bool sensorPresent = false;
static bool batteryTempPresent = false;
static bool loadTempPresent = false;
static Adafruit_MCP9600 *batteryTempSensor = nullptr;
static Adafruit_MCP9600 *loadTempSensor = nullptr;

static constexpr uint8_t BATTERY_TEMP_ADDRESS = 0x67;
static constexpr uint8_t LOAD_TEMP_ADDRESS = 0x64;
static constexpr uint8_t DAC_ADDRESS = 0x4C;
static constexpr uint16_t DAC_STARTUP_CODE_3Q_FS = 49152;


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


static void configureMCP960x(Adafruit_MCP9600 &sensor)
{
  sensor.setADCresolution(MCP9600_ADCRESOLUTION_18);
  sensor.setThermocoupleType(MCP9600_TYPE_K);
  sensor.setFilterCoefficient(3);
}


static bool initializeMCP960x(Adafruit_MCP9600 &sensor9600, Adafruit_MCP9601 &sensor9601, Adafruit_MCP9600 **activeSensor, uint8_t address, const char *name)
{
  if (activeSensor == nullptr) {
    return false;
  }

  *activeSensor = nullptr;

  if (sensor9600.begin(address, &Wire)) {
    configureMCP960x(sensor9600);
    *activeSensor = &sensor9600;
    Serial.printf("%s detected as MCP9600 at 0x%02X (Type K).\n", name, address);
    return true;
  }

  if (sensor9601.begin(address, &Wire)) {
    configureMCP960x(sensor9601);
    *activeSensor = &sensor9601;
    Serial.printf("%s detected as MCP9601 at 0x%02X (Type K).\n", name, address);
    return true;
  }

  return false;
}


static bool initializeAD5693()
{
  if (!ad5693.begin(DAC_ADDRESS, &Wire)) {
    return false;
  }

  ad5693.reset();
  if (!ad5693.setMode(NORMAL_MODE, true, true)) {
    return false;
  }

  if (!ad5693.writeUpdateDAC(DAC_STARTUP_CODE_3Q_FS)) {
    return false;
  }

  return true;
}


static float celsiusToFahrenheit(float celsius)
{
  return (celsius * 9.0f / 5.0f) + 32.0f;
}


static float readBatteryTempF(uint32_t nowMs)
{
  if (!batteryTempPresent && (nowMs - lastBatteryTempRetryMs >= 2000)) {
    lastBatteryTempRetryMs = nowMs;
    batteryTempPresent = initializeMCP960x(mcp9600Battery, mcp9601Battery, &batteryTempSensor, BATTERY_TEMP_ADDRESS, "Battery thermocouple");
    if (batteryTempPresent) {
      Serial.println("Battery thermocouple detected and initialized.");
    } else {
      Serial.printf("Battery thermocouple not detected at 0x%02X.\n", BATTERY_TEMP_ADDRESS);
    }
  }

  if (!batteryTempPresent || batteryTempSensor == nullptr) {
    return NAN;
  }

  float tempC = batteryTempSensor->readThermocouple();
  if (isnan(tempC)) {
    batteryTempPresent = false;
    batteryTempSensor = nullptr;
    Serial.println("Battery thermocouple read failed.");
    return NAN;
  }

  return celsiusToFahrenheit(tempC);
}


static float readLoadTempF(uint32_t nowMs)
{
  if (!loadTempPresent && (nowMs - lastLoadTempRetryMs >= 2000)) {
    lastLoadTempRetryMs = nowMs;
    loadTempPresent = initializeMCP960x(mcp9600Load, mcp9601Load, &loadTempSensor, LOAD_TEMP_ADDRESS, "Load thermocouple");
    if (loadTempPresent) {
      Serial.println("Load thermocouple detected and initialized.");
    } else {
      Serial.printf("Load thermocouple not detected at 0x%02X.\n", LOAD_TEMP_ADDRESS);
    }
  }

  if (!loadTempPresent || loadTempSensor == nullptr) {
    return NAN;
  }

  float tempC = loadTempSensor->readThermocouple();
  if (isnan(tempC)) {
    loadTempPresent = false;
    loadTempSensor = nullptr;
    Serial.println("Load thermocouple read failed.");
    return NAN;
  }

  return celsiusToFahrenheit(tempC);
}


void updateINA3221AndUi()
{
  uint32_t nowMs = millis();
  uint32_t deltaMs = (lastSampleMs == 0) ? 0 : (nowMs - lastSampleMs);
  float deltaHours = (float)deltaMs / 3600000.0f;
  float batteryTempF = readBatteryTempF(nowMs);
  float loadTempF = readLoadTempF(nowMs);

  if (!sensorPresent) {
    for (uint8_t i = 0; i < 3; i++) {
      if (ui_consume_reset_request(i)) {
        energyWh[i] = 0.0f;
      }

      channelData[i].voltage_v = 0.0f;
      channelData[i].current_ma = 0.0f;
      channelData[i].power_w = 0.0f;
      channelData[i].energy_wh = energyWh[i];
      channelData[i].battery_temp_f = batteryTempF;
      channelData[i].load_temp_f = loadTempF;

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
    channelData[i].battery_temp_f = batteryTempF;
    channelData[i].load_temp_f = loadTempF;

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

    batteryTempPresent = initializeMCP960x(mcp9600Battery, mcp9601Battery, &batteryTempSensor, BATTERY_TEMP_ADDRESS, "Battery thermocouple");
    if (batteryTempPresent) {
      Serial.println("Battery thermocouple initialized.");
    } else {
      Serial.println("Battery thermocouple not found at startup.");
    }

    loadTempPresent = initializeMCP960x(mcp9600Load, mcp9601Load, &loadTempSensor, LOAD_TEMP_ADDRESS, "Load thermocouple");
    if (loadTempPresent) {
      Serial.println("Load thermocouple initialized.");
    } else {
      Serial.println("Load thermocouple not found at startup.");
    }

    if (initializeAD5693()) {
      Serial.printf("AD5693 initialized at 0x%02X, output set to ~75%% FS (%u).\n", DAC_ADDRESS, DAC_STARTUP_CODE_3Q_FS);
    } else {
      Serial.printf("AD5693 not detected/configured at 0x%02X.\n", DAC_ADDRESS);
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
      "CH1: %.3fV %.2fmA %.3fW %.6fWh | CH2: %.3fV %.2fmA %.3fW %.6fWh | CH3: %.3fV %.2fmA %.3fW %.6fWh | Batt: %.1fF Load: %.1fF\n",
      channelData[0].voltage_v, channelData[0].current_ma, channelData[0].power_w, channelData[0].energy_wh,
      channelData[1].voltage_v, channelData[1].current_ma, channelData[1].power_w, channelData[1].energy_wh,
      channelData[2].voltage_v, channelData[2].current_ma, channelData[2].power_w, channelData[2].energy_wh,
      channelData[0].battery_temp_f, channelData[0].load_temp_f
    );
    lastDebugMs = now;
  }

  delay(5);

}
