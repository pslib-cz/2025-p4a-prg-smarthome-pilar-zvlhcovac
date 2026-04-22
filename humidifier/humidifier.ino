/*
 * Smart Humidifier — ESP32-2432S028 (CYD)
 * =========================================
 * Adaptováno z old_project_sketch.ino (termokamera pro 3D tiskárnu)
 *
 * Hardware:
 *   - ESP32-2432S028 (CYD)
 *   - SHT30 senzor (teplota + vlhkost) na I²C (SDA=22, SCL=27, addr=0x44)
 *   - PCF8574 I/O expander na I²C (addr=0x27, bit 0 = STDZ-1810 zvlhčovač)
 *   - ILI9341 display (TFT_eSPI, 320×240)
 *   - XPT2046 touchscreen (VSPI, IRQ=36, MOSI=32, MISO=39, CLK=25, CS=33)
 *
 * Funkce:
 *   - Standalone regulace vlhkosti (hysteréza ±2 %), funguje bez WiFi/HA
 *   - MQTT přes TLS — reporting + příjímání příkazů z HA
 *   - MQTT Discovery — entity se automaticky vytvoří v Home Assistant
 *   - Preferences/NVS — cílová vlhkost přežije restart
 *   - LVGL UI — aktuální hodnoty, target ±, status, offline banner
 */

// ============================================================
// INCLUDES
// ============================================================
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <lvgl.h>
#include <Wire.h>
#include "PCF8574.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <Preferences.h>

// ============================================================
// SECRETS — zkopíruj secrets_template.h na secrets.h a vyplň
// ============================================================
#include "secrets.h"
// secrets.h musí definovat:
//   WIFI_SSID, WIFI_PASSWORD
//   MQTT_BROKER, MQTT_PORT (8883), MQTT_USER, MQTT_PASS
//   MQTT_CA_CERT (PEM string)

// ============================================================
// TOUCHSCREEN PINOUT (identické se starým projektem)
// ============================================================
#define XPT2046_IRQ  36
#define XPT2046_MOSI 32   // VSPI2 / HSPI — separate from TFT
#define XPT2046_MISO 39
#define XPT2046_CLK  25
#define XPT2046_CS   33

// ============================================================
// I²C
// ============================================================
#define I2C_SDA 22
#define I2C_SCL 27

// ============================================================
// PCF8574 — STDZ-1810 zvlhčovač na bitu 0
// Zkontroluj adresu dle jumperu A0/A1/A2 na modulu!
// Starý projekt: 0x27 (A0=A1=A2=HIGH); pouprav dle potřeby.
// ============================================================
PCF8574 pcf(0x27);
#define HUMIDIFIER_BIT 0

// ============================================================
// SHT30 PŘÍKAZY
// ============================================================
#define SHT30_ADDR         0x44
#define SHT30_MEAS_HIGHREP 0x2C06  // single shot, high repeatability, clock stretch

// ============================================================
// DISPLAY + TOUCH
// ============================================================
TFT_eSPI tft = TFT_eSPI();

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

#define DRAW_BUF_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT / 20 * (LV_COLOR_DEPTH / 8))
static uint32_t draw_buf[DRAW_BUF_SIZE / 4];

SPIClass touchscreen_SPI = SPIClass(HSPI);  // same hardware SPI as TFT, separate CS
XPT2046_Touchscreen touchscreen(XPT2046_CS);  // no IRQ — GPIO36 floats LOW on CYD, causing false triggers

int touch_x, touch_y, touch_z;

// ============================================================
// MQTT + WiFi klienti
// ============================================================
WiFiClientSecure secure_client;
PubSubClient mqtt_client(secure_client);

// ============================================================
// NVS
// ============================================================
Preferences prefs;

// ============================================================
// MQTT TOPICS
// ============================================================
#define MQTT_PREFIX       "humidifier"
#define MQTT_STATUS       MQTT_PREFIX "/status"
#define MQTT_HUM          MQTT_PREFIX "/sensor/humidity"
#define MQTT_TEMP         MQTT_PREFIX "/sensor/temperature"
#define MQTT_ACTIVE       MQTT_PREFIX "/binary/active"
#define MQTT_RSSI         MQTT_PREFIX "/sensor/rssi"
#define MQTT_UPTIME       MQTT_PREFIX "/sensor/uptime"
#define MQTT_POWER_STATE  MQTT_PREFIX "/switch/power"
#define MQTT_POWER_SET    MQTT_PREFIX "/switch/power/set"
#define MQTT_TARGET_STATE MQTT_PREFIX "/number/target"
#define MQTT_TARGET_SET   MQTT_PREFIX "/number/target/set"

// ============================================================
// GLOBÁLNÍ PROMĚNNÉ
// ============================================================
float current_humidity  = NAN;
float current_temp      = NAN;
float target_humidity   = 50.0f;
bool  humidifier_on     = false;
bool  auto_mode         = true;
bool  wifi_connected    = false;
bool  mqtt_connected    = false;
bool  night_mode        = false;

// pro stavový display
char buf[64];

// ============================================================
// LVGL WIDGETY
// ============================================================
lv_obj_t *label_humidity;
lv_obj_t *label_temperature;
lv_obj_t *label_target;
lv_obj_t *label_status;
lv_obj_t *label_conn;
lv_obj_t *label_night;
lv_obj_t *btn_plus;
lv_obj_t *btn_minus;

// ============================================================
// SHT30 ČTENÍ (upraveno z get_I2C_reading() starého projektu)
// ============================================================
struct SensorReading {
  float temp;
  float hum;
};

SensorReading read_sht30() {
  // Odešli měřicí příkaz (2 byty)
  Wire.beginTransmission(SHT30_ADDR);
  Wire.write((uint8_t)(SHT30_MEAS_HIGHREP >> 8));   // 0x2C
  Wire.write((uint8_t)(SHT30_MEAS_HIGHREP & 0xFF));  // 0x06
  if (Wire.endTransmission() != 0) {
    return { NAN, NAN };
  }

  delay(20); // SHT30 potřebuje ~15 ms pro high repeatability měření

  Wire.requestFrom((uint8_t)SHT30_ADDR, (uint8_t)6);
  if (Wire.available() < 6) {
    return { NAN, NAN };
  }

  uint8_t data[6];
  for (int i = 0; i < 6; i++) data[i] = Wire.read();

  // Výpočet teplot/vlhkosti (dle SHT30 datasheet)
  uint16_t raw_temp = ((uint16_t)data[0] << 8) | data[1];
  uint16_t raw_hum  = ((uint16_t)data[3] << 8) | data[4];

  float temperature = 175.0f * ((float)raw_temp / 65535.0f) - 45.0f;
  float humidity    = 100.0f * ((float)raw_hum  / 65535.0f);

  return { temperature, humidity };
}

// ============================================================
// REGULAČNÍ LOGIKA
// ============================================================
void update_control() {
  if (isnan(current_humidity)) {
    // Bezpečnostní stop — I²C selhalo, nelze regulovat
    pcf.write(HUMIDIFIER_BIT, LOW);
    humidifier_on = false;
    return;
  }

  if (!auto_mode) {
    // Manuální override — stav zvlhčovače řídí jen MQTT příkaz
    return;
  }

  if (current_humidity < target_humidity - 2.0f) {
    pcf.write(HUMIDIFIER_BIT, HIGH);
    humidifier_on = true;
  } else if (current_humidity > target_humidity + 2.0f) {
    pcf.write(HUMIDIFIER_BIT, LOW);
    humidifier_on = false;
  }
  // V hysteréze ±2 % — ponecháme stávající stav
}

// ============================================================
// MQTT CALLBACK (příkazy z HA)
// ============================================================
void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  String t = String(topic);

  if (t == MQTT_TARGET_SET) {
    float val = msg.toFloat();
    if (val >= 30.0f && val <= 80.0f) {
      target_humidity = val;
      // Ulož do NVS
      prefs.begin("humid", false);
      prefs.putFloat("target", target_humidity);
      prefs.end();
    }
  } else if (t == MQTT_POWER_SET) {
    if (msg == "ON") {
      pcf.write(HUMIDIFIER_BIT, HIGH);
      humidifier_on = true;
      auto_mode = false;
    } else if (msg == "OFF") {
      pcf.write(HUMIDIFIER_BIT, LOW);
      humidifier_on = false;
      auto_mode = false;
    } else if (msg == "AUTO") {
      auto_mode = true;
    }
  }
}

// ============================================================
// MQTT DISCOVERY — registrace entit v HA
// ============================================================
void publish_discovery() {
  // Helper: retention = true, aby HA entity přejily restart brokeru
  auto pub = [](const char* topic, String payload) {
    mqtt_client.publish(topic, payload.c_str(), true);
  };

  const String avail = ",\"availability_topic\":\"" MQTT_STATUS "\"";
  const String dev   = ",\"device\":{\"identifiers\":[\"humidifier_cyd\"],"
                       "\"name\":\"Smart Humidifier\",\"model\":\"ESP32-CYD\","
                       "\"manufacturer\":\"DIY\"}";

  // Vlhkost
  pub("homeassistant/sensor/humidifier/humidity/config",
    "{\"name\":\"Vlhkost\",\"unique_id\":\"hum_humidity\","
    "\"state_topic\":\"" MQTT_HUM "\","
    "\"unit_of_measurement\":\"%\",\"device_class\":\"humidity\","
    "\"icon\":\"mdi:water-percent\"" + avail + dev + "}");

  // Teplota
  pub("homeassistant/sensor/humidifier/temperature/config",
    "{\"name\":\"Teplota\",\"unique_id\":\"hum_temperature\","
    "\"state_topic\":\"" MQTT_TEMP "\","
    "\"unit_of_measurement\":\"°C\",\"device_class\":\"temperature\","
    "\"icon\":\"mdi:thermometer\"" + avail + dev + "}");

  // Relé switch
  pub("homeassistant/switch/humidifier/power/config",
    "{\"name\":\"Zvlhčovač\",\"unique_id\":\"hum_power\","
    "\"state_topic\":\"" MQTT_POWER_STATE "\","
    "\"command_topic\":\"" MQTT_POWER_SET "\","
    "\"payload_on\":\"ON\",\"payload_off\":\"OFF\","
    "\"icon\":\"mdi:air-humidifier\"" + avail + dev + "}");

  // Cílová vlhkost (number)
  pub("homeassistant/number/humidifier/target/config",
    "{\"name\":\"Cílová vlhkost\",\"unique_id\":\"hum_target\","
    "\"state_topic\":\"" MQTT_TARGET_STATE "\","
    "\"command_topic\":\"" MQTT_TARGET_SET "\","
    "\"min\":30,\"max\":80,\"step\":1,"
    "\"unit_of_measurement\":\"%\","
    "\"icon\":\"mdi:target\"" + avail + dev + "}");

  // Binary sensor — zvlhčovač aktivní
  pub("homeassistant/binary_sensor/humidifier/active/config",
    "{\"name\":\"Zvlhčovač aktivní\",\"unique_id\":\"hum_active\","
    "\"state_topic\":\"" MQTT_ACTIVE "\","
    "\"payload_on\":\"ON\",\"payload_off\":\"OFF\","
    "\"device_class\":\"running\","
    "\"icon\":\"mdi:water\"" + avail + dev + "}");

  // WiFi RSSI
  pub("homeassistant/sensor/humidifier/rssi/config",
    "{\"name\":\"WiFi RSSI\",\"unique_id\":\"hum_rssi\","
    "\"state_topic\":\"" MQTT_RSSI "\","
    "\"unit_of_measurement\":\"dBm\",\"device_class\":\"signal_strength\","
    "\"entity_category\":\"diagnostic\"" + avail + dev + "}");

  // Uptime
  pub("homeassistant/sensor/humidifier/uptime/config",
    "{\"name\":\"Uptime\",\"unique_id\":\"hum_uptime\","
    "\"state_topic\":\"" MQTT_UPTIME "\","
    "\"unit_of_measurement\":\"s\",\"device_class\":\"duration\","
    "\"entity_category\":\"diagnostic\"" + avail + dev + "}");
}

// ============================================================
// MQTT PUBLISH — odeslání aktuálního stavu
// ============================================================
void publish_state() {
  if (!mqtt_client.connected()) return;

  if (!isnan(current_humidity)) {
    snprintf(buf, sizeof(buf), "%.1f", current_humidity);
    mqtt_client.publish(MQTT_HUM, buf, true);
  }
  if (!isnan(current_temp)) {
    snprintf(buf, sizeof(buf), "%.1f", current_temp);
    mqtt_client.publish(MQTT_TEMP, buf, true);
  }

  mqtt_client.publish(MQTT_ACTIVE,       humidifier_on ? "ON" : "OFF", true);
  mqtt_client.publish(MQTT_POWER_STATE,  humidifier_on ? "ON" : "OFF", true);

  snprintf(buf, sizeof(buf), "%.0f", target_humidity);
  mqtt_client.publish(MQTT_TARGET_STATE, buf, true);

  snprintf(buf, sizeof(buf), "%d", WiFi.RSSI());
  mqtt_client.publish(MQTT_RSSI, buf);

  snprintf(buf, sizeof(buf), "%lu", millis() / 1000UL);
  mqtt_client.publish(MQTT_UPTIME, buf);
}

// ============================================================
// MQTT RECONNECT
// ============================================================
void mqtt_reconnect() {
  if (mqtt_client.connected() || !wifi_connected) return;

  // LWT (Last Will and Testament) — HA dostane "offline" při ztrátě spojení
  if (mqtt_client.connect("humidifier_cyd", MQTT_USER, MQTT_PASS,
                           MQTT_STATUS, 1, true, "offline")) {
    mqtt_connected = true;
    mqtt_client.publish(MQTT_STATUS, "online", true);

    // Přihlásit se na příkazové topicy
    mqtt_client.subscribe(MQTT_TARGET_SET);
    mqtt_client.subscribe(MQTT_POWER_SET);

    // Zregistruj entity v HA
    publish_discovery();
    publish_state();
  } else {
    mqtt_connected = false;
  }
}

// ============================================================
// LVGL — čtení touchscreenu (stejné jako starý projekt)
// ============================================================
void touchscreen_read(lv_indev_t *indev, lv_indev_data_t *data) {
  if (touchscreen.touched()) {
    TS_Point p = touchscreen.getPoint();

    Serial.printf("[TOUCH] raw=(%d,%d) z=%d\n", p.x, p.y, p.z);

    // Discard obviously invalid readings (SPI noise / floating bus)
    if (p.x < 100 || p.x > 4000 || p.y < 100 || p.y > 4000) {
      data->state = LV_INDEV_STATE_RELEASED;
      return;
    }

    int x_vis = map(p.x, 3757, 248, 0, SCREEN_WIDTH - 1);
    int y_vis = map(p.y, 3526, 371, 0, SCREEN_HEIGHT - 1);
    
    x_vis = constrain(x_vis, 0, SCREEN_WIDTH - 1);
    y_vis = constrain(y_vis, 0, SCREEN_HEIGHT - 1);

    // LVGL is rotated 270 degrees. It expects physical portrait coords (240x320)
    // and transforms them to logical landscape coords (Logical_X = Phys_Y, Logical_Y = 239 - Phys_X)
    // Inverse transformation: Phys_X = 239 - Logical_Y, Phys_Y = Logical_X
    touch_x = (SCREEN_HEIGHT - 1) - y_vis;
    touch_y = x_vis;

    Serial.printf("[TOUCH] raw=(%d,%d) -> visual=(%d,%d) -> lvgl_phys=(%d,%d)\n", p.x, p.y, x_vis, y_vis, touch_x, touch_y);

    data->state    = LV_INDEV_STATE_PRESSED;
    data->point.x = touch_x;
    data->point.y = touch_y;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

// ============================================================
// LVGL — event handler tlačítek
// ============================================================
static void btn_event_handler(lv_event_t *e) {
  lv_event_code_t code   = lv_event_get_code(e);
  lv_obj_t       *target = (lv_obj_t *)lv_event_get_target(e);

  if (code == LV_EVENT_CLICKED) {
    if (target == btn_plus) {
      if (target_humidity < 80.0f) {
        target_humidity += 1.0f;
        prefs.begin("humid", false);
        prefs.putFloat("target", target_humidity);
        prefs.end();
        // Informuj HA
        snprintf(buf, sizeof(buf), "%.0f", target_humidity);
        mqtt_client.publish(MQTT_TARGET_STATE, buf, true);
      }
    } else if (target == btn_minus) {
      if (target_humidity > 30.0f) {
        target_humidity -= 1.0f;
        prefs.begin("humid", false);
        prefs.putFloat("target", target_humidity);
        prefs.end();
        snprintf(buf, sizeof(buf), "%.0f", target_humidity);
        mqtt_client.publish(MQTT_TARGET_STATE, buf, true);
      }
    }
  }
}

// ============================================================
// LVGL — sestavení GUI
// ============================================================
void setup_gui() {
  lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x001428), LV_PART_MAIN);

  // --- Hlavní sekce vlhkosti (vlevo nahoře) ---
  lv_obj_t *lbl_hum_title = lv_label_create(lv_screen_active());
  lv_label_set_text(lbl_hum_title, "HUMIDITY");
  lv_obj_set_style_text_color(lbl_hum_title, lv_color_hex(0x5DADE2), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl_hum_title, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_align(lbl_hum_title, LV_ALIGN_TOP_LEFT, 10, 8);

  label_humidity = lv_label_create(lv_screen_active());
  lv_label_set_text(label_humidity, "---.--%");
  lv_obj_set_style_text_color(label_humidity, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_text_font(label_humidity, &lv_font_montserrat_48, LV_PART_MAIN);
  lv_obj_align(label_humidity, LV_ALIGN_TOP_LEFT, 10, 26);

  // --- Teplota (vpravo nahoře) ---
  lv_obj_t *lbl_temp_title = lv_label_create(lv_screen_active());
  lv_label_set_text(lbl_temp_title, "TEMPERATURE");
  lv_obj_set_style_text_color(lbl_temp_title, lv_color_hex(0xF39C12), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl_temp_title, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_align(lbl_temp_title, LV_ALIGN_TOP_RIGHT, -10, 8);

  label_temperature = lv_label_create(lv_screen_active());
  lv_label_set_text(label_temperature, "--.-°C");
  lv_obj_set_style_text_color(label_temperature, lv_color_hex(0xF39C12), LV_PART_MAIN);
  lv_obj_set_style_text_font(label_temperature, &lv_font_montserrat_28, LV_PART_MAIN);
  lv_obj_align(label_temperature, LV_ALIGN_TOP_RIGHT, -10, 30);

  // --- Oddělovací čára ---
  lv_obj_t *line_sep = lv_obj_create(lv_screen_active());
  lv_obj_set_size(line_sep, SCREEN_WIDTH - 20, 2);
  lv_obj_set_style_bg_color(line_sep, lv_color_hex(0x2E4057), LV_PART_MAIN);
  lv_obj_set_style_border_width(line_sep, 0, LV_PART_MAIN);
  lv_obj_align(line_sep, LV_ALIGN_TOP_MID, 0, 95);

  // --- Cílová vlhkost + tlačítka ---
  lv_obj_t *lbl_target_title = lv_label_create(lv_screen_active());
  lv_label_set_text(lbl_target_title, "TARGET HUMIDITY");
  lv_obj_set_style_text_color(lbl_target_title, lv_color_hex(0xAAAAAA), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl_target_title, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_align(lbl_target_title, LV_ALIGN_TOP_LEFT, 10, 105);

  label_target = lv_label_create(lv_screen_active());
  lv_label_set_text(label_target, "50%");
  lv_obj_set_style_text_color(label_target, lv_color_hex(0x2ECC71), LV_PART_MAIN);
  lv_obj_set_style_text_font(label_target, &lv_font_montserrat_28, LV_PART_MAIN);
  lv_obj_align(label_target, LV_ALIGN_TOP_LEFT, 10, 123);

  // Tlačítko [−]
  btn_minus = lv_btn_create(lv_screen_active());
  lv_obj_add_event_cb(btn_minus, btn_event_handler, LV_EVENT_ALL, NULL);
  lv_obj_set_size(btn_minus, 55, 45);
  lv_obj_set_style_bg_color(btn_minus, lv_color_hex(0xC0392B), LV_PART_MAIN);
  lv_obj_set_style_radius(btn_minus, 8, LV_PART_MAIN);
  lv_obj_align(btn_minus, LV_ALIGN_TOP_RIGHT, -70, 100);
  lv_obj_t *lbl_m = lv_label_create(btn_minus);
  lv_label_set_text(lbl_m, "-");
  lv_obj_set_style_text_font(lbl_m, &lv_font_montserrat_28, LV_PART_MAIN);
  lv_obj_center(lbl_m);

  // Tlačítko [+]
  btn_plus = lv_btn_create(lv_screen_active());
  lv_obj_add_event_cb(btn_plus, btn_event_handler, LV_EVENT_ALL, NULL);
  lv_obj_set_size(btn_plus, 55, 45);
  lv_obj_set_style_bg_color(btn_plus, lv_color_hex(0x27AE60), LV_PART_MAIN);
  lv_obj_set_style_radius(btn_plus, 8, LV_PART_MAIN);
  lv_obj_align(btn_plus, LV_ALIGN_TOP_RIGHT, -10, 100);
  lv_obj_t *lbl_p = lv_label_create(btn_plus);
  lv_label_set_text(lbl_p, "+");
  lv_obj_set_style_text_font(lbl_p, &lv_font_montserrat_28, LV_PART_MAIN);
  lv_obj_center(lbl_p);

  // --- Status zvlhčovače ---
  label_status = lv_label_create(lv_screen_active());
  lv_label_set_text(label_status, LV_SYMBOL_STOP " STANDBY");
  lv_obj_set_style_text_color(label_status, lv_color_hex(0x555555), LV_PART_MAIN);
  lv_obj_set_style_text_font(label_status, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_align(label_status, LV_ALIGN_BOTTOM_LEFT, 10, -30);

  // --- Připojení (WiFi/MQTT) ---
  label_conn = lv_label_create(lv_screen_active());
  lv_label_set_text(label_conn, LV_SYMBOL_WIFI " Connecting...");
  lv_obj_set_style_text_color(label_conn, lv_color_hex(0xAAAAAA), LV_PART_MAIN);
  lv_obj_set_style_text_font(label_conn, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_align(label_conn, LV_ALIGN_BOTTOM_LEFT, 10, -10);

  // --- Noční režim banner (skrytý) ---
  label_night = lv_label_create(lv_screen_active());
  lv_label_set_text(label_night, LV_SYMBOL_BELL " NIGHT MODE");
  lv_obj_set_style_text_color(label_night, lv_color_hex(0x8E44AD), LV_PART_MAIN);
  lv_obj_set_style_text_font(label_night, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_align(label_night, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
  lv_obj_add_flag(label_night, LV_OBJ_FLAG_HIDDEN);
}

// ============================================================
// LVGL — aktualizace dispeje (voláno periodicky)
// ============================================================
void update_display() {
  // Vlhkost
  if (!isnan(current_humidity)) {
    snprintf(buf, sizeof(buf), "%.1f%%", current_humidity);
  } else {
    snprintf(buf, sizeof(buf), "ERR");
  }
  lv_label_set_text(label_humidity, buf);

  // Teplota
  if (!isnan(current_temp)) {
    snprintf(buf, sizeof(buf), "%.1f\u00b0C", current_temp);
  } else {
    snprintf(buf, sizeof(buf), "---");
  }
  lv_label_set_text(label_temperature, buf);

  // Cílová vlhkost
  snprintf(buf, sizeof(buf), "%.0f%%", target_humidity);
  lv_label_set_text(label_target, buf);

  // Status zvlhčovače
  if (isnan(current_humidity)) {
    lv_label_set_text(label_status, LV_SYMBOL_WARNING " SENSOR ERROR");
    lv_obj_set_style_text_color(label_status, lv_color_hex(0xE74C3C), LV_PART_MAIN);
  } else if (humidifier_on) {
    lv_label_set_text(label_status, LV_SYMBOL_PLAY " ACTIVE");
    lv_obj_set_style_text_color(label_status, lv_color_hex(0x2ECC71), LV_PART_MAIN);
  } else {
    lv_label_set_text(label_status, LV_SYMBOL_STOP " STANDBY");
    lv_obj_set_style_text_color(label_status, lv_color_hex(0x555555), LV_PART_MAIN);
  }

  // Připojení
  if (!wifi_connected) {
    lv_label_set_text(label_conn, LV_SYMBOL_WIFI " OFFLINE");
    lv_obj_set_style_text_color(label_conn, lv_color_hex(0xE74C3C), LV_PART_MAIN);
  } else if (!mqtt_connected) {
    lv_label_set_text(label_conn, LV_SYMBOL_WIFI " WiFi OK  MQTT...");
    lv_obj_set_style_text_color(label_conn, lv_color_hex(0xF39C12), LV_PART_MAIN);
  } else {
    lv_label_set_text(label_conn, LV_SYMBOL_WIFI " Online");
    lv_obj_set_style_text_color(label_conn, lv_color_hex(0x27AE60), LV_PART_MAIN);
  }

  // Noční bannner
  if (night_mode) {
    lv_obj_clear_flag(label_night, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(label_night, LV_OBJ_FLAG_HIDDEN);
  }
}

// ============================================================
// SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("=== Smart Humidifier boot ===");

  // I²C
  Wire.begin(I2C_SDA, I2C_SCL, 100000);
  Serial.println("[OK] I2C");

  // PCF8574 — inicializace, při startu zvlhčovač OFF
  pcf.begin();
  pcf.write(HUMIDIFIER_BIT, LOW);
  Serial.println("[OK] PCF8574");

  // NVS — načti uloženou cílovou vlhkost
  prefs.begin("humid", true);
  target_humidity = prefs.getFloat("target", 50.0f);
  prefs.end();
  Serial.printf("[OK] NVS  target=%.0f%%\n", target_humidity);

  // TFT + Touch inicializace (stejné jako starý projekt)
  touchscreen_SPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touchscreen.begin(touchscreen_SPI);
  touchscreen.setRotation(1);

  tft.init();
  tft.setRotation(1);
  Serial.println("[OK] TFT + touch");

  // LVGL inicializace (stejné jako starý projekt)
  lv_init();
  lv_display_t *disp = lv_tft_espi_create(SCREEN_HEIGHT, SCREEN_WIDTH, draw_buf, sizeof(draw_buf));
  lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_270);

  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, touchscreen_read);
  Serial.println("[OK] LVGL");

  // GUI
  setup_gui();
  Serial.println("[OK] GUI");

  // WiFi
  Serial.printf("[WiFi] Connecting to SSID: '%s'\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  // MQTT TLS — Povolení připojení přes IP adresu navzdory certifikátu pro doménu.
  // Zajišťuje šifrování, ale nekontroluje shodu názvu (tzv. "Bypass").
  secure_client.setInsecure(); 

  mqtt_client.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt_client.setCallback(mqtt_callback);
  mqtt_client.setBufferSize(1024); // větší buffer pro Discovery JSON
  Serial.println("[OK] MQTT configured");
}

// ============================================================
// LOOP
// ============================================================
int ticks = 0;

void loop() {
  // Sledování WiFi
  wifi_connected = (WiFi.status() == WL_CONNECTED);

  // MQTT reconnect každých ~5 s pokud odpojeno
  if (wifi_connected && ticks % 5000 == 0) {
    if (!mqtt_client.connected()) {
      mqtt_connected = false;
      mqtt_reconnect();
    }
  }

  // Zpracování příchozích MQTT zpráv
  if (mqtt_connected) {
    mqtt_client.loop();
    mqtt_connected = mqtt_client.connected();
  }

  // Senzory + ovládání každých ~2 s
  if (ticks % 2000 == 0) {
    SensorReading reading = read_sht30();
    current_temp     = reading.temp;
    current_humidity = reading.hum;
    update_control();
    update_display();
  }

  // MQTT reporting každých ~10 s
  if (ticks % 10000 == 0 && mqtt_connected) {
    publish_state();
  }

  // LVGL (stejné jako starý projekt)
  lv_task_handler();
  lv_tick_inc(50);

  ticks += 50;
  if (ticks >= 100000) ticks = 0; // reset aby int nepřetekl

  delay(50);
}
