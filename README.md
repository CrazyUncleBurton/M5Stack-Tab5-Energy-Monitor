# M5Stack-Tab5-Energy-Monitor
M5Stack Tab5 Energy Monitor for TI INA3221 Current Sensor Squareline LVGL UI
by Bryan A. "CrazyUncleBurton" Thompson
Last Updated 2/28/2026

## To Do

1. Add support for thermocouples (load and battery) and add graphs to battery mode.  Add check for hardware at startup.
2. Add support for DAC. Add check for hardware at startup.
3. Add Settings screen.  Configure logging, RTC, other stuff here.
4. Implement RTC. How do we set that? Add to screen.
5. Log to SD Card if present and a box is checked - create CSV. 
6. Add Battery Measurement Mode.  CH1.  Reporting via LCD.  Control Current by controlling ADC. 
7. Add ADCx8 to measure indiv cells on a balance connector.
8. Add settings for min battery discharge voltage, battery chemistry, number of cells in series, ampacity for pass/fail.

## Concepts

In this project we show you how to download a project from GitHub, then build the project and upload to the M5Stack / Tab5 microcontroller.  The program will read from a TI INA3221 sensor via I2C and then output the data to the microcontroller LCD. The UI has been updated to an LVGL / Squareline Studio project.

## Hardware

- Microcontroller:  M5Stack Tab5 (ESP32-P4NRW32@RISC-V 32-bit Dual-core 360MHz + LP Single-core 40MHz)
- Display:  5" (1280 x 720) IPS TFT LCD and ILI9881C controller with GT911 capacitive touch controller
- Current and Voltage Sensors: Adafruit INA3221
- Thermocouple Amplifiers:  MCP9600
- DAC:  AD5693R

## I2C Map

External Bus / PORTA (Red):
- Adafruit INA3221 Current Sensor - 0x40
- Adafruit INA228 Current Sensor - 0x40
- Adafruit MCP9600 Thermocouple Amplifier - 0x67
- Adafruit AD5693R DAC - 0x4C (addr pin Low) and 0x4E (addr pin High)

## Project Documentation

See the project files / docs folder for a PDF of the tutorial.

## Sensors

### Texas Instruments INA32211 Sensor by Adafruit.com

Connect the cable to the pins on the sensor:
- Sensor VCC -> Tab5 Red PORTA Red/VCC
- Sensor GND -> Tab5 Red PORTA Black/GND
- Sensor SDA -> Tab5 Red PORTA Yellow/SDA
- Sensor SCL -> Tab5 Red PORTA White/SCL

Then connect cable to the PORTA (the red port on the microcontroller) which is connected to the External I2C bus.  It is referenced as Wire(); in Arduino.

The library comes from Adafruit.com.  It can read voltage and current on three independent circuits, and then calculate power and other statistics from the data it gathers.  Each can be inserted either high-side or low-side.  

It can measure voltage up to 26V with a resolution of 8mV/step (this is a Bus Voltage measurement).

It can measure +/-3.2A with a resolution of 0.390625mA/step (this is a Shunt measurement) with its supplied 0.05 Ohm shunt resistors, or it can be modified for larger or smaller current ranges.  

The default I2C address for the Adafruit board is 0x40.

### Texas Instruments INA228 Current Sensor by Adafruit.com

The default I2C address for the Adafruit board is 0x40.

### Microchip MCP9600 Thermocouple Amplifiers by Adafruit.com

The default I2C address for the Adafruit board is 0x67 (address pin tied to VIN by the manufacturer).  Tie address to ground for 0x60.  

### Analog Devices AD5693R 16-bit DAC by Adafruit.com

The default I2C address for the Adafruit board is 0x4C (address pin connected low by default).  Connect address pin high for the alternate I2C address of 0x4E.

## M5Stack Tab5 Dev Board Information

### Turning the M5Stack Tab5 On and Off

1. Press the square white button once to turn on the device.

2. Press the square white button twice to turn off the device.

### The Battery

The battery connections on the back of the M5Stack Tab5 are a standard Sony NP-F Lithium Ion battery (pretty much any capacity).  

The battery only charges when the device is on and configured.

### Programming

1. Program like you would any other VS Code Project.  Click the PlatformIO:Upload button (shaped like a right arrow).

2. If it doesn't work, enter Download Mode - With USB cable or battery connected, long‑press the Reset button (2 seconds) until the internal green LED rapidly blinks; release to enter download mode and await firmware flashing.

### M5Stack Tab5 Pin Map

#### Camera

- G32 - Camera SCL
- G33 - Camera SDA
- G36 - Camera MCLK
- CSI_DATAP1 (Dedicated) - CAM_D1P
- CSI_DATAN1 (Dedicated) - CAM_D1N
- CSI_CLKP (Dedicated) - CAM_CSI_CKP
- CSI_CLKN (Dedicated) - CAM_CSI_CKN
- CSI_DATAP0 (Dedicated) - CSI_DOP
- CSI_DATAN0 (Dedicated) - CSI_DON

#### ES8388 2 Channel Audio Codec

ES8388 (0x10)
- G30 - MCLK
- G27 - SCLK
- G26 - DSDIN
- G29 - LRCK
- G32 - SCL
- G33 - SDA

#### ES7210 4 CH ADC 24-bit Possible Mic Array

ES7210 (0x40)
- G30 - MCLK
- G27 - SCLK
- G26 - ASDOUT
- G29 - LRCK
- G32 - SCL
- G33 - SDA

#### LCD ILI9881C (Old) / ST7123 (New post 2024)

- G22 - LEDA
- DSI_CLKN (Dedicated) - DSI_CK_N
- DSI_CLKP (Dedicated) - DSI_CK_P
- DSI_DATAN1 (Dedicated) - DSI_D1_N
- DSI_DATAP1 (Dedicated) - DSI_D1_P
- DSI_DATAN0 (Dedicated) - DSI_D0_N
- DSI_DATAP0 (Dedicated) - DSI_D0_P

#### Touch

GT911 (0x14) / ST7123 (0x55)
- G31 - SDA
- G32 - SCL
- G23 - TP_INT
- E1.P5 - TP_RST

#### BMI270

BMI270 (0x68)
- G32 - SCL
- G31 - SDA

#### RTC(RX8130CE)

RX8130CE (0x32)
- G32 - SCL
- G31 - SDA

#### INA226

INA226 (0x40)
- G32 - SCL
- G31 - SDA

#### ESP32-C6

- G11 - SDIO2_D0
- G10 - SDIO2_D1
- G9 - SDIO2_D2
- G8 - SDIO2_D3
- G13 - SDIO2_CMD
- G12 - SDIO2_CK
- G15 - RESET
- G14 - IO2
- G35 - BOOT

#### microSD

microSD SPI Mode
- G9 - MISO
- G42 - CS
- G43 - SCK
- G44 - MOSI

microSD SDIO Mode
- G39 - DAT0
- G40 - DAT1
- G41 - DAT2
- G42 - DAT3
- G43 - CLK
- G44 - CMD

#### RS485

SIT3088
- G21 - RX
- G20 - TX
- G34 - DIR

#### HY2.0-4P PORT A

The red external grove connector marked PORTA is connected to Wire() by default - not to Wire1().  This is now referenced as M5.Ex_I2C.begin(); in the M5Unified library.

- Black - GND
- Red - +5V
- Yellow - G53 - Ext SDA
- White - G54 - Ext SCL

#### I2C (Internal)

This is referenced by Wire1() in Arduino / PlatformIO with Arduino framework. This is now referenced as   M5.In_I2C.begin(); in the M5Unified library.

- G32 - Int SCL
- G33 - Int SDA

#### I2C (External - Port A)

This is referenced by Wire() in Arduino / PlatformIO with Arduino framework. This is now referenced as M5.Ex_I2C.begin(); in the M5Unified library.

- G53 - Ext SCL
- G54 - Ext SDA

#### SPI

- G18 - MOSI
- G19 - MISO
- G5 - SCK

#### Serial

- G38 - RXD0
- G37 - TXD0

#### M5Bus (Rear)

Note:  The bus is the same layout as the old CORE2 bus with old GPIO map
- Pin 1 - GND
- Pin 2 - G16 - GPIO
- Pin 3 - GND
- Pin 4 - G17 - PB_IN
- Pin 5 - GND
- Pin 6 - RST - EN
- Pin 7 - G18 - SPI_MOSI
- Pin 8 - G45 - GPIO
- Pin 9 - G19 - SPI_MISO
- Pin 10 - G52 - PB_OUT
- Pin 11 - G5 - SPI_SCK
- Pin 12 - 3V3
- Pin 13 - G38 - RXD0
- Pin 14 - G37 - TXD0
- Pin 15 - G7 - PC-RX
- Pin 16 - G6 - PC_TX
- Pin 17 - G31 - Int SDA
- Pin 18 - G32 - Int SCL
- Pin 19 - G3 - GPIO
- Pin 20 - G4 - GPIO
- Pin 21 - G2 - GPIO
- Pin 22 - G48 - GPIO
- Pin 23 - G47 - GPIO
- Pin 24 - G35 - GPIO - BOOT
- Pin 25 - HVIN
- Pin 26 - G51 - GPIO
- Pin 27 - HVIN
- Pin 28 - 5V
- Pin 29 - HVIN
- Pin 30 - BAT

#### Ext Port 1 (Side)

- G50
- G1
- 3V3
- GND
- HVIN
- GND
- GND
- EXT 5V
- G0
- G49

#### Ext Port 2 - RS485 (Rear)

- Pin 1 - Black - GND
- Pin 2 - Red - HVIN
- Pin 3 - YEL - 485A
- Pin 4 - Green - 485B
- Pin 5 - White - SDA - G32
- Pin 6 - Blue - SCL - G32

#### USB C Ext (Side)

- Pin 1 - USB1_D+
- Pin 2 - USB1_D-
- Pin 3 - GND
- Pin 4 - 5VIN

## ESP32 Arduino Core 3.x Info

This project updates the Arduino Core to v3.x.  This enables all kinds of things in ESP32 like the high-res GPTimer, the high-res RMT transmit/receive peripheral, and FreeRTOS.  Make sure the platform= line in the platformio.ini file looks like this:

platform = https://github.com/pioarduino/platform-espressif32.git#54.03.20; (Arduino Core 3.x)

If your other non-core3 projects stop building after this update, it may be because they don't explicitly state that they are intended for an older version of the Arduino Core. To do that, update the platformio.ini file and replace the platform line in the older project(s) with this:

platform = espressif32@~6.5.0; (or whatever version you want. 6.5–6.8 map to Arduino core 2.0.x)

## References

Dev Board Info:
<https://docs.m5stack.com/en/core/Tab5>

Microcontroller Info:
<https://www.espressif.com/en/products/socs/esp32-p4>
<https://www.espressif.com/en/support/documents/technical-documents?keys=&field_type_tid_parent=esp32P4Series-SoCs&field_type_tid%5B%5D=1633>

Adafruit INA3221 Sensor Board Info:
<https://learn.adafruit.com/adafruit-ina3221-breakout>

Texas Instruments INA3221 Sensor Data Sheet:
<https://www.ti.com/lit/ds/symlink/ina3221.pdf>

M5GFX Display Library:
<https://docs.m5stack.com/en/arduino/m5gfx/m5gfx_functions>

LVGL:
<https://lvgl.io/>

SquareLine Studios UI Creator (we need v8.33-8.4):
<https://squareline.io/downloads>
