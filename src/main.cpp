#include <Arduino.h>
#include "WS_MQTT.h"
#include "WS_Bluetooth.h"
#include "WS_GPIO.h"
#include "WS_RTC.h"
#include "WS_SDM230.h"

// SDM230 on UART1, RS485 pins defined in WS_GPIO.h
// Device address 1, 9600 baud (SDM230 factory default)
static SDM230 meter(UART_NUM_1, TXD1, RXD1, TXD1EN, 1, 9600);

static void publishMeterData(const SDM230Data& d);

void setup() {
  Serial.begin(115200);
  vTaskDelay(pdMS_TO_TICKS(1000));

  printf("--- ESP32-S3-Relay-1CH ---\r\n");
  printf("Firmware Version: 1.0.0\r\n");

  I2C_Init();
  RTC_Init();

  if (!meter.begin()) {
    printf("SDM230: UART init failed\r\n");
  }

  MQTT_Init();
  Bluetooth_Init();
  Relay_Init();

  printf("SDM230 Modbus reader ready\r\n");
}

void loop() {
  static uint32_t lastRead = 0;
  uint32_t now = (uint32_t)(esp_timer_get_time() / 1000); // ms since boot

  if (now - lastRead >= 10000) {
    lastRead = now;

    SDM230Data data;
    if (meter.read(data)) {
      publishMeterData(data);
    } else {
      printf("SDM230: read failed\r\n");
    }
  }

  vTaskDelay(pdMS_TO_TICKS(10));
}

static void publishMeterData(const SDM230Data& d) {
  printf("V=%.2f A=%.3f W=%.2f PF=%.3f Hz=%.2f kWh=%.3f\r\n",
         d.voltage, d.current, d.activePower,
         d.powerFactor, d.frequency, d.totalEnergy);

  JsonDocument json;
  json["voltage"]       = d.voltage;
  json["current"]       = d.current;
  json["active_power"]  = d.activePower;
  json["apparent_power"]= d.apparentPower;
  json["reactive_power"]= d.reactivePower;
  json["power_factor"]  = d.powerFactor;
  json["frequency"]     = d.frequency;
  json["import_energy"] = d.importEnergy;
  json["export_energy"] = d.exportEnergy;
  json["total_energy"]  = d.totalEnergy;

  sendJsonData(json);
}