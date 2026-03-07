/*
  Tab5 INA3221 Energy Monitor

  by Bryan A. "CrazyUncleBurton" Thompson

  Last Updated 3/6/2026
*/

#include <Wire.h>
#include <Preferences.h>
#include <M5UnitUnified.h>
#include <M5UnitUnifiedMETER.h>
#include <M5UnitUnifiedANADIG.h>
#include <M5GFX.h>
#include <math.h>
#include "lvgl.h"
#include "ui/ui.h"
#include "pins_config.h"

// Sensor objects
static m5::unit::UnitUnified meterUnits1A;
static m5::unit::UnitUnified meterUnits10A;
static m5::unit::UnitUnified thermoUnits;
static m5::unit::UnitUnified anadigUnits;

static m5::unit::UnitINA226_1A ina226_1a;
static m5::unit::UnitINA226_10A ina226_10a;
static m5::unit::UnitKmeterISO loadThermocouple;
static m5::unit::UnitDAC2 dac2;

static m5::unit::UnitINA226 *activeIna226 = &ina226_1a;
static m5::unit::UnitUnified *activeMeterUnits = &meterUnits1A;
static Preferences preferences;

// Display
static M5GFX display;

// LVGL buffers
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *buf;

// Runtime state
static ui_channel_data_t channelData;
static ui_config_t runtimeConfig = {
  UI_SENSOR_INA226_1A,
  UI_UNITS_IMPERIAL,
  UI_BATTERY_LIFEPO4,
  UI_LOAD_CONSTANT_CURRENT,
  UI_GRAPH_TRACE_ALL,
  200,
  10.0f,
  60.0f,
  100.0f,
  4
};

static uint32_t lastSampleMs = 0;
static uint32_t lastDebugMs = 0;
static uint32_t lastLvTickMs = 0;
static uint32_t lastSensorRetryMs = 0;
static uint32_t lastLoadTempRetryMs = 0;

static float energyWh = 0.0f;
static bool sensor1AReady = false;
static bool sensor10AReady = false;
static bool loadTempPresent = false;
static bool sensorPresent = false;
static bool cutoffReached = false;
static bool overtempReached = false;
static bool dacPresent = false;
static bool testRunning = false;

static constexpr uint8_t DAC_ADDRESS = 0x59;
static constexpr const char *PREF_NAMESPACE = "energy_cfg";

static void publishSensorStatus(void)
{
  char status[192];
  snprintf(status, sizeof(status),
           "INA226-1A: %s\n"
           "INA226-10A: %s\n"
           "Load Thermocouple: %s\n"
           "DAC2: %s",
           sensor1AReady ? "Connected" : "Not Connected",
           sensor10AReady ? "Connected" : "Not Connected",
           loadTempPresent ? "Connected" : "Not Connected",
           dacPresent ? "Connected" : "Not Connected");
  ui_set_sensor_status(status);
}

static void sanitizeConfig(ui_config_t *config)
{
  if (config == nullptr) {
    return;
  }

  if (config->sensor_type != UI_SENSOR_INA226_1A && config->sensor_type != UI_SENSOR_INA226_10A) {
    config->sensor_type = UI_SENSOR_INA226_1A;
  }

  if (config->units != UI_UNITS_IMPERIAL && config->units != UI_UNITS_METRIC) {
    config->units = UI_UNITS_IMPERIAL;
  }

  if (config->sample_interval_ms < 50) {
    config->sample_interval_ms = 50;
  }
  if (config->sample_interval_ms > 2000) {
    config->sample_interval_ms = 2000;
  }

  if (config->cutoff_voltage_v < 1.0f) {
    config->cutoff_voltage_v = 1.0f;
  }
  if (config->cutoff_voltage_v > 150.0f) {
    config->cutoff_voltage_v = 150.0f;
  }

  if (config->overtemp_cutoff_c < 30.0f) {
    config->overtemp_cutoff_c = 30.0f;
  }
  if (config->overtemp_cutoff_c > 100.0f) {
    config->overtemp_cutoff_c = 100.0f;
  }

  if (config->battery_type < UI_BATTERY_ALKALINE || config->battery_type > UI_BATTERY_LIFEPO4) {
    config->battery_type = UI_BATTERY_LIFEPO4;
  }

  if (config->load_type < UI_LOAD_CONSTANT_CURRENT || config->load_type > UI_LOAD_PULSED) {
    config->load_type = UI_LOAD_CONSTANT_CURRENT;
  }

  config->graph_trace_mask &= UI_GRAPH_TRACE_ALL;
  if (config->graph_trace_mask == 0) {
    config->graph_trace_mask = UI_GRAPH_TRACE_ALL;
  }

  if (config->rated_battery_ampacity_ah < 0.1f) {
    config->rated_battery_ampacity_ah = 0.1f;
  }
  if (config->rated_battery_ampacity_ah > 500.0f) {
    config->rated_battery_ampacity_ah = 500.0f;
  }

  if (config->num_series_cells < 1) {
    config->num_series_cells = 1;
  }
  if (config->num_series_cells > 30) {
    config->num_series_cells = 30;
  }
}

static void saveConfigToNvs(const ui_config_t *config)
{
  if (config == nullptr) {
    return;
  }

  preferences.putUChar("sensor", (uint8_t)config->sensor_type);
  preferences.putUChar("units", (uint8_t)config->units);
  preferences.putUChar("battery", (uint8_t)config->battery_type);
  preferences.putUChar("loadtype", (uint8_t)config->load_type);
  preferences.putUChar("graphmask", config->graph_trace_mask);
  preferences.putUShort("sample", config->sample_interval_ms);
  preferences.putFloat("cutoff", config->cutoff_voltage_v);
  preferences.putFloat("overtemp", config->overtemp_cutoff_c);
  preferences.putFloat("ampacity", config->rated_battery_ampacity_ah);
  preferences.putUChar("cells", config->num_series_cells);
}

static void loadConfigFromNvs(ui_config_t *config)
{
  if (config == nullptr) {
    return;
  }

  config->sensor_type = (ui_sensor_type_t)preferences.getUChar("sensor", (uint8_t)config->sensor_type);
  config->units = (ui_units_t)preferences.getUChar("units", (uint8_t)config->units);
  config->battery_type = (ui_battery_type_t)preferences.getUChar("battery", (uint8_t)config->battery_type);
  config->load_type = (ui_load_type_t)preferences.getUChar("loadtype", (uint8_t)config->load_type);
  config->graph_trace_mask = preferences.getUChar("graphmask", config->graph_trace_mask);
  config->sample_interval_ms = preferences.getUShort("sample", config->sample_interval_ms);
  config->cutoff_voltage_v = preferences.getFloat("cutoff", config->cutoff_voltage_v);
  config->overtemp_cutoff_c = preferences.getFloat("overtemp", config->overtemp_cutoff_c);
  config->rated_battery_ampacity_ah = preferences.getFloat("ampacity", config->rated_battery_ampacity_ah);
  config->num_series_cells = preferences.getUChar("cells", config->num_series_cells);

  sanitizeConfig(config);
}

static float celsiusToFahrenheit(float celsius)
{
  return (celsius * 9.0f / 5.0f) + 32.0f;
}

static void setSensorPresent(bool present)
{
  sensorPresent = present;
  publishSensorStatus();
}

static const char *sensorTypeName(ui_sensor_type_t sensorType)
{
  return sensorType == UI_SENSOR_INA226_10A ? "INA226-10A" : "INA226-1A";
}

static bool initializeINA226_1A()
{
  if (sensor1AReady) {
    return true;
  }
  if (!meterUnits1A.add(ina226_1a, Wire)) {
    return false;
  }
  sensor1AReady = meterUnits1A.begin();
  return sensor1AReady;
}

static bool initializeINA226_10A()
{
  if (sensor10AReady) {
    return true;
  }
  if (!meterUnits10A.add(ina226_10a, Wire)) {
    return false;
  }
  sensor10AReady = meterUnits10A.begin();
  return sensor10AReady;
}

static bool initializeLoadThermocouple()
{
  if (loadTempPresent) {
    return true;
  }
  if (!thermoUnits.add(loadThermocouple, Wire)) {
    return false;
  }
  loadTempPresent = thermoUnits.begin();
  return loadTempPresent;
}

static bool initializeDAC2()
{
  if (dacPresent) {
    return true;
  }

  auto cfg = dac2.config();
  cfg.range0 = m5::unit::gp8413::Output::Range10V;
  cfg.range1 = m5::unit::gp8413::Output::Range10V;
  dac2.config(cfg);

  if (!anadigUnits.add(dac2, Wire)) {
    Serial.println("DAC2 add() failed.");
    return false;
  }

  dacPresent = anadigUnits.begin();
  if (!dacPresent) {
    Serial.println("DAC2 begin() failed.");
    return false;
  }

  return true;
}

static void applyDacTestPattern()
{
  if (!dacPresent) {
    return;
  }

  constexpr float ch0_test_mv = 1500.0f;
  constexpr float ch1_test_mv = 8500.0f;

  bool wrote = dac2.writeBothVoltage(ch0_test_mv, ch1_test_mv);
  if (wrote) {
    Serial.printf("DAC2 test output set: CH0=%.3fV CH1=%.3fV (Range 0-10V).\n", ch0_test_mv / 1000.0f, ch1_test_mv / 1000.0f);
    Serial.println("DAC2 is unipolar only; negative voltage output is not supported by this hardware.");
  } else {
    Serial.println("DAC2 write failed.");
  }
}

static void applySensorSelection(void)
{
  if (runtimeConfig.sensor_type == UI_SENSOR_INA226_10A) {
    activeIna226 = &ina226_10a;
    activeMeterUnits = &meterUnits10A;
    setSensorPresent(sensor10AReady);
  } else {
    activeIna226 = &ina226_1a;
    activeMeterUnits = &meterUnits1A;
    setSensorPresent(sensor1AReady);
  }
}

static void applyUiConfig(const ui_config_t *config)
{
  if (config == nullptr) {
    return;
  }

  bool sensorChanged = (runtimeConfig.sensor_type != config->sensor_type);
  runtimeConfig = *config;

  sanitizeConfig(&runtimeConfig);

  applySensorSelection();
  saveConfigToNvs(&runtimeConfig);

  if (sensorChanged) {
    lastSampleMs = 0;
    cutoffReached = false;
    overtempReached = false;
    Serial.printf("Sensor type changed to %s\n", sensorTypeName(runtimeConfig.sensor_type));
  }
}

static float readLoadTempConfiguredUnits(uint32_t nowMs)
{
  if (!loadTempPresent && (nowMs - lastLoadTempRetryMs >= 2000)) {
    lastLoadTempRetryMs = nowMs;
    loadTempPresent = initializeLoadThermocouple();
    if (loadTempPresent) {
      Serial.println("Load thermocouple detected and initialized.");
    } else {
      Serial.println("Load thermocouple not detected.");
    }
  }

  if (!loadTempPresent) {
    return NAN;
  }

  thermoUnits.update();
  float tempC = loadThermocouple.temperature();
  if (isnan(tempC)) {
    loadTempPresent = false;
    Serial.println("Load thermocouple read failed.");
    return NAN;
  }

  if (runtimeConfig.units == UI_UNITS_METRIC) {
    return tempC;
  }

  return celsiusToFahrenheit(tempC);
}

static void updateEnergyAndUi()
{
  uint32_t nowMs = millis();
  uint32_t deltaMs = (lastSampleMs == 0) ? 0 : (nowMs - lastSampleMs);
  float deltaHours = (float)deltaMs / 3600000.0f;

  if (ui_consume_start_request(0)) {
    energyWh = 0.0f;
    testRunning = true;
    lastSampleMs = nowMs;
    cutoffReached = false;
    overtempReached = false;
    channelData.energy_wh = 0.0f;
    ui_set_test_running(true);
  }

  if (ui_consume_stop_request(0)) {
    testRunning = false;
    lastSampleMs = nowMs;
    ui_set_test_running(false);
  }

  if (ui_consume_reset_request(0)) {
    energyWh = 0.0f;
    testRunning = false;
    cutoffReached = false;
    overtempReached = false;
    lastSampleMs = nowMs;
    deltaMs = 0;
    deltaHours = 0.0f;
    ui_set_test_running(false);
  }

  if (!testRunning) {
    return;
  }

  float loadTemp = readLoadTempConfiguredUnits(nowMs);

  if (!sensorPresent || activeMeterUnits == nullptr || activeIna226 == nullptr) {
    channelData.voltage_v = 0.0f;
    channelData.current_ma = 0.0f;
    channelData.power_w = 0.0f;
    channelData.energy_wh = energyWh;
    channelData.load_temp_f = loadTemp;
    ui_set_channel_data(0, &channelData);
    lastSampleMs = nowMs;
    return;
  }

  activeMeterUnits->update();

  float voltageV = activeIna226->voltage() / 1000.0f;
  float currentA = activeIna226->current() / 1000.0f;

  if (isnan(voltageV) || isnan(currentA)) {
    setSensorPresent(false);
    Serial.println("INA226 read failed, sensor unavailable.");
    lastSampleMs = nowMs;
    return;
  }

  if (!cutoffReached && voltageV <= runtimeConfig.cutoff_voltage_v) {
    cutoffReached = true;
    Serial.printf("Cutoff reached at %.3fV (configured %.3fV).\n", voltageV, runtimeConfig.cutoff_voltage_v);
  }

  // Check for overtemp cutoff (temperature is already in correct units)
  if (!isnan(loadTemp) && !overtempReached) {
    float tempC = (runtimeConfig.units == UI_UNITS_METRIC) ? loadTemp : (loadTemp - 32.0f) * 5.0f / 9.0f;
    if (tempC >= runtimeConfig.overtemp_cutoff_c) {
      overtempReached = true;
      Serial.printf("Overtemp cutoff reached at %.1fC (configured %.1fC).\n", tempC, runtimeConfig.overtemp_cutoff_c);
    }
  }

  float powerW = voltageV * currentA;
  if (testRunning && !cutoffReached && !overtempReached && deltaMs > 0) {
    energyWh += powerW * deltaHours;
  }

  channelData.voltage_v = voltageV;
  channelData.current_ma = currentA * 1000.0f;
  channelData.power_w = powerW;
  channelData.energy_wh = energyWh;
  channelData.load_temp_f = loadTemp;

  ui_set_channel_data(0, &channelData);
  lastSampleMs = nowMs;
}

void lv_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  display.pushImageDMA(area->x1, area->y1, w, h, (uint16_t *)&color_p->full);
  lv_disp_flush_ready(disp);
}

static void lv_indev_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data)
{
  (void)indev_driver;

  lgfx::touch_point_t tp[3];
  uint8_t touchpad = display.getTouch(tp, 3);
  if (touchpad > 0) {
    data->state = LV_INDEV_STATE_PR;
    data->point.x = tp[0].x;
    data->point.y = tp[0].y;
  } else {
    data->state = LV_INDEV_STATE_REL;
  }

  data->continue_reading = false;
}

void setup()
{
  display.init();
  Serial.begin(115200);

  if (!preferences.begin(PREF_NAMESPACE, false)) {
    Serial.println("Failed to open NVS preferences namespace.");
  }
  loadConfigFromNvs(&runtimeConfig);
  saveConfigToNvs(&runtimeConfig);

  lv_init();
  buf = (lv_color_t *)heap_caps_malloc(sizeof(lv_color_t) * LVGL_LCD_BUF_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  lv_disp_draw_buf_init(&draw_buf, buf, NULL, LVGL_LCD_BUF_SIZE);

  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = EXAMPLE_LCD_H_RES;
  disp_drv.ver_res = EXAMPLE_LCD_V_RES;
  disp_drv.flush_cb = lv_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  disp_drv.sw_rotate = 0;
  lv_disp_drv_register(&disp_drv);

  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = lv_indev_read;
  lv_indev_drv_register(&indev_drv);

  ui_init();
  ui_set_config(&runtimeConfig);
  ui_set_test_running(false);
  display.setBrightness(255);

  Wire.begin(I2C_SDA, I2C_SCL, 400000U);
  delay(1000);

  (void)initializeINA226_1A();
  (void)initializeINA226_10A();
  applySensorSelection();

  if (!sensorPresent) {
    Serial.printf("%s not found at startup, running without sensor data.\n", sensorTypeName(runtimeConfig.sensor_type));
  } else {
    Serial.printf("%s initialized.\n", sensorTypeName(runtimeConfig.sensor_type));
  }

  if (initializeLoadThermocouple()) {
    Serial.println("Load thermocouple initialized.");
  } else {
    Serial.println("Load thermocouple not found at startup.");
  }

  if (initializeDAC2()) {
    Serial.printf("DAC2 detected at expected I2C address 0x%02X.\n", DAC_ADDRESS);
    applyDacTestPattern();
  } else {
    Serial.printf("DAC2 not detected at expected I2C address 0x%02X.\n", DAC_ADDRESS);
  }

  publishSensorStatus();

  updateEnergyAndUi();
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

  ui_config_t updatedConfig;
  if (ui_consume_config_update(&updatedConfig)) {
    applyUiConfig(&updatedConfig);
  }

  if (!sensorPresent && (now - lastSensorRetryMs >= 2000)) {
    lastSensorRetryMs = now;

    if (runtimeConfig.sensor_type == UI_SENSOR_INA226_10A) {
      (void)initializeINA226_10A();
    } else {
      (void)initializeINA226_1A();
    }

    applySensorSelection();
    if (sensorPresent) {
      Serial.printf("%s detected and reinitialized.\n", sensorTypeName(runtimeConfig.sensor_type));
      lastSampleMs = 0;
    } else {
      Serial.printf("%s still not detected.\n", sensorTypeName(runtimeConfig.sensor_type));
    }
  }

  if (now - lastSampleMs >= runtimeConfig.sample_interval_ms) {
    updateEnergyAndUi();
  }

  if (now - lastDebugMs >= 1000) {
    publishSensorStatus();
    Serial.printf(
      "Sensor:%s | V:%.3fV I:%.2fmA P:%.3fW E:%.6fWh | Load:%.1f%s | Cutoff:%.2fV %s | Overtemp:%.0fC %s | Batt:%uAh %ucells\\n",
      sensorTypeName(runtimeConfig.sensor_type),
      channelData.voltage_v, channelData.current_ma, channelData.power_w, channelData.energy_wh,
      channelData.load_temp_f, runtimeConfig.units == UI_UNITS_METRIC ? "C" : "F",
      runtimeConfig.cutoff_voltage_v, cutoffReached ? "REACHED" : "OK",
      runtimeConfig.overtemp_cutoff_c, overtempReached ? "REACHED" : "OK",
      (unsigned)runtimeConfig.rated_battery_ampacity_ah, (unsigned)runtimeConfig.num_series_cells
    );
    lastDebugMs = now;
  }

  delay(5);
}
