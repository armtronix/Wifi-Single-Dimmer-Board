/*
  sonoff.ino - Sonoff-Tasmota firmware for iTead Sonoff, Wemos and NodeMCU hardware

  Copyright (C) 2018  Theo Arends

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
/*====================================================
  Prerequisites:
    - Change libraries/PubSubClient/src/PubSubClient.h
        #define MQTT_MAX_PACKET_SIZE 1000

    - Select IDE Tools - Flash Mode: "DOUT"
    - Select IDE Tools - Flash Size: "1M (no SPIFFS)"
  ====================================================*/

// Location specific includes
#include <core_version.h>                   // Arduino_Esp8266 version information (ARDUINO_ESP8266_RELEASE and ARDUINO_ESP8266_RELEASE_2_3_0)
#include "sonoff_version.h"                 // Sonoff-Tasmota version information
#include "sonoff.h"                         // Enumeration used in my_user_config.h
#include "my_user_config.h"                 // Fixed user configurable options
#ifdef USE_CONFIG_OVERRIDE
  #include "user_config_override.h"         // Configuration overrides for my_user_config.h
#endif
#include "sonoff_post.h"                    // Configuration overrides for all previous includes
#include "i18n.h"                           // Language support configured by my_user_config.h
#include "sonoff_template.h"                // Hardware configuration

#ifdef ARDUINO_ESP8266_RELEASE_2_4_0
#include "lwip/init.h"
#if LWIP_VERSION_MAJOR != 1
  #error Please use stable lwIP v1.4
#endif
#endif

// Libraries
#include <Ticker.h>                         // RTC, Energy, OSWatch
#include <ESP8266WiFi.h>                    // MQTT, Ota, WifiManager
#include <ESP8266HTTPClient.h>              // MQTT, Ota
#include <ESP8266httpUpdate.h>              // Ota
#include <StreamString.h>                   // Webserver, Updater
#include <ArduinoJson.h>                    // WemoHue, IRremote, Domoticz
#ifdef USE_WEBSERVER
  #include <ESP8266WebServer.h>             // WifiManager, Webserver
  #include <DNSServer.h>                    // WifiManager
#endif  // USE_WEBSERVER
#ifdef USE_ARDUINO_OTA
  #include <ArduinoOTA.h>                   // Arduino OTA
  #ifndef USE_DISCOVERY
  #define USE_DISCOVERY
  #endif
#endif  // USE_ARDUINO_OTA
#ifdef USE_DISCOVERY
  #include <ESP8266mDNS.h>                  // MQTT, Webserver, Arduino OTA
#endif  // USE_DISCOVERY
#ifdef USE_I2C
  #include <Wire.h>                         // I2C support library
#endif  // USE_I2C
#ifdef USE_SPI
  #include <SPI.h>                          // SPI support, TFT
#endif  // USE_SPI

// Structs
#include "settings.h"

enum TasmotaCommands {
  CMND_BACKLOG, CMND_DELAY, CMND_POWER, CMND_FANSPEED, CMND_STATUS, CMND_STATE, CMND_POWERONSTATE, CMND_PULSETIME,
  CMND_BLINKTIME, CMND_BLINKCOUNT, CMND_SENSOR, CMND_SAVEDATA, CMND_SETOPTION, CMND_TEMPERATURE_RESOLUTION, CMND_HUMIDITY_RESOLUTION,
  CMND_PRESSURE_RESOLUTION, CMND_POWER_RESOLUTION, CMND_VOLTAGE_RESOLUTION, CMND_FREQUENCY_RESOLUTION, CMND_CURRENT_RESOLUTION, CMND_ENERGY_RESOLUTION, CMND_WEIGHT_RESOLUTION,
  CMND_MODULE, CMND_MODULES, CMND_GPIO, CMND_GPIOS, CMND_PWM, CMND_PWMFREQUENCY, CMND_PWMRANGE, CMND_COUNTER, CMND_COUNTERTYPE,
  CMND_COUNTERDEBOUNCE, CMND_BUTTONDEBOUNCE, CMND_SWITCHDEBOUNCE, CMND_SLEEP, CMND_UPGRADE, CMND_UPLOAD, CMND_OTAURL, CMND_SERIALLOG, CMND_SYSLOG,
  CMND_LOGHOST, CMND_LOGPORT, CMND_IPADDRESS, CMND_NTPSERVER, CMND_AP, CMND_SSID, CMND_PASSWORD, CMND_HOSTNAME,
  CMND_WIFICONFIG, CMND_FRIENDLYNAME, CMND_SWITCHMODE, CMND_SETSENSOR,
  CMND_TELEPERIOD, CMND_RESTART, CMND_RESET, CMND_TIMEZONE, CMND_TIMESTD, CMND_TIMEDST, CMND_ALTITUDE, CMND_LEDPOWER, CMND_LEDSTATE,
  CMND_I2CSCAN, CMND_SERIALSEND, CMND_BAUDRATE, CMND_SERIALDELIMITER, CMND_DRIVER };
const char kTasmotaCommands[] PROGMEM =
  D_CMND_BACKLOG "|" D_CMND_DELAY "|" D_CMND_POWER "|" D_CMND_FANSPEED "|" D_CMND_STATUS "|" D_CMND_STATE "|"  D_CMND_POWERONSTATE "|" D_CMND_PULSETIME "|"
  D_CMND_BLINKTIME "|" D_CMND_BLINKCOUNT "|" D_CMND_SENSOR "|" D_CMND_SAVEDATA "|" D_CMND_SETOPTION "|" D_CMND_TEMPERATURE_RESOLUTION "|" D_CMND_HUMIDITY_RESOLUTION "|"
  D_CMND_PRESSURE_RESOLUTION "|" D_CMND_POWER_RESOLUTION "|" D_CMND_VOLTAGE_RESOLUTION "|" D_CMND_FREQUENCY_RESOLUTION "|" D_CMND_CURRENT_RESOLUTION "|" D_CMND_ENERGY_RESOLUTION "|" D_CMND_WEIGHT_RESOLUTION "|"
  D_CMND_MODULE "|" D_CMND_MODULES "|" D_CMND_GPIO "|" D_CMND_GPIOS "|" D_CMND_PWM "|" D_CMND_PWMFREQUENCY "|" D_CMND_PWMRANGE "|" D_CMND_COUNTER "|" D_CMND_COUNTERTYPE "|"
  D_CMND_COUNTERDEBOUNCE "|" D_CMND_BUTTONDEBOUNCE "|" D_CMND_SWITCHDEBOUNCE "|" D_CMND_SLEEP "|" D_CMND_UPGRADE "|" D_CMND_UPLOAD "|" D_CMND_OTAURL "|" D_CMND_SERIALLOG "|" D_CMND_SYSLOG "|"
  D_CMND_LOGHOST "|" D_CMND_LOGPORT "|" D_CMND_IPADDRESS "|" D_CMND_NTPSERVER "|" D_CMND_AP "|" D_CMND_SSID "|" D_CMND_PASSWORD "|" D_CMND_HOSTNAME "|"
  D_CMND_WIFICONFIG "|" D_CMND_FRIENDLYNAME "|" D_CMND_SWITCHMODE "|" D_CMND_SETSENSOR "|"
  D_CMND_TELEPERIOD "|" D_CMND_RESTART "|" D_CMND_RESET "|" D_CMND_TIMEZONE "|" D_CMND_TIMESTD "|" D_CMND_TIMEDST "|" D_CMND_ALTITUDE "|" D_CMND_LEDPOWER "|" D_CMND_LEDSTATE "|"
  D_CMND_I2CSCAN "|" D_CMND_SERIALSEND "|" D_CMND_BAUDRATE "|" D_CMND_SERIALDELIMITER "|" D_CMND_DRIVER;

const uint8_t kIFan02Speed[4][3] = {{6,6,6}, {7,6,6}, {7,7,6}, {7,6,7}};

// Global variables
SerialConfig serial_config = SERIAL_8N1;    // Serial interface configuration 8 data bits, No parity, 1 stop bit

#ifdef USE_MQTT_TLS
  WiFiClientSecure EspClient;               // Wifi Secure Client
#else
  WiFiClient EspClient;                     // Wifi Client
#endif

WiFiUDP PortUdp;                            // UDP Syslog and Alexa

unsigned long feature_drv1;                 // Compiled driver feature map
unsigned long feature_drv2;                 // Compiled driver feature map
unsigned long feature_sns1;                 // Compiled sensor feature map
unsigned long feature_sns2;                 // Compiled sensor feature map
unsigned long serial_polling_window = 0;    // Serial polling window
unsigned long state_second = 0;             // State second timer
unsigned long state_50msecond = 0;          // State 50msecond timer
unsigned long state_100msecond = 0;         // State 100msecond timer
unsigned long state_250msecond = 0;         // State 250msecond timer
unsigned long pulse_timer[MAX_PULSETIMERS] = { 0 }; // Power off timer
unsigned long blink_timer = 0;              // Power cycle timer
unsigned long backlog_delay = 0;            // Command backlog delay
unsigned long button_debounce = 0;          // Button debounce timer
unsigned long switch_debounce = 0;          // Switch debounce timer
power_t power = 0;                          // Current copy of Settings.power
power_t blink_power;                        // Blink power state
power_t blink_mask = 0;                     // Blink relay active mask
power_t blink_powersave;                    // Blink start power save state
power_t latching_power = 0;                 // Power state at latching start
power_t rel_inverted = 0;                   // Relay inverted flag (1 = (0 = On, 1 = Off))
int baudrate = APP_BAUDRATE;                // Serial interface baud rate
int serial_in_byte_counter = 0;             // Index in receive buffer
int ota_state_flag = 0;                     // OTA state flag
int ota_result = 0;                         // OTA result
int restart_flag = 0;                       // Sonoff restart flag
int wifi_state_flag = WIFI_RESTART;         // Wifi state flag
int tele_period = 1;                        // Tele period timer
int blinks = 201;                           // Number of LED blinks
uint32_t uptime = 0;                        // Counting every second until 4294967295 = 130 year
uint32_t global_update = 0;                 // Timestamp of last global temperature and humidity update
float global_temperature = 0;               // Provide a global temperature to be used by some sensors
float global_humidity = 0;                  // Provide a global humidity to be used by some sensors
char *ota_url;                              // OTA url string pointer
uint16_t dual_button_code = 0;              // Sonoff dual received code
uint16_t mqtt_cmnd_publish = 0;             // ignore flag for publish command
uint16_t blink_counter = 0;                 // Number of blink cycles
uint16_t seriallog_timer = 0;               // Timer to disable Seriallog
uint16_t syslog_timer = 0;                  // Timer to re-enable syslog_level
uint16_t holdbutton[MAX_KEYS] = { 0 };      // Timer for button hold
uint16_t switch_no_pullup = 0;              // Switch pull-up bitmask flags
int16_t save_data_counter;                  // Counter and flag for config save to Flash
RulesBitfield rules_flag;                   // Rule state flags (16 bits)
uint8_t serial_local = 0;                   // Handle serial locally;
uint8_t fallback_topic_flag = 0;            // Use Topic or FallbackTopic
uint8_t state_250mS = 0;                    // State 250msecond per second flag
uint8_t latching_relay_pulse = 0;           // Latching relay pulse timer
uint8_t backlog_index = 0;                  // Command backlog index
uint8_t backlog_pointer = 0;                // Command backlog pointer
uint8_t backlog_mutex = 0;                  // Command backlog pending
uint8_t interlock_mutex = 0;                // Interlock power command pending
uint8_t sleep;                              // Current copy of Settings.sleep
uint8_t stop_flash_rotate = 0;              // Allow flash configuration rotation
uint8_t blinkstate = 0;                     // LED state
uint8_t blinkspeed = 1;                     // LED blink rate
uint8_t lastbutton[MAX_KEYS] = { NOT_PRESSED, NOT_PRESSED, NOT_PRESSED, NOT_PRESSED };  // Last button states
uint8_t multiwindow[MAX_KEYS] = { 0 };      // Max time between button presses to record press count
uint8_t multipress[MAX_KEYS] = { 0 };       // Number of button presses within multiwindow
uint8_t lastwallswitch[MAX_SWITCHES];       // Last wall switch states
uint8_t holdwallswitch[MAX_SWITCHES] = { 0 };  // Timer for wallswitch push button hold
uint8_t virtualswitch[MAX_SWITCHES];        // Virtual switch states
uint8_t pin[GPIO_MAX];                      // Possible pin configurations
uint8_t led_inverted = 0;                   // LED inverted flag (1 = (0 = On, 1 = Off))
uint8_t pwm_inverted = 0;                   // PWM inverted flag (1 = inverted)
uint8_t counter_no_pullup = 0;              // Counter input pullup flag (1 = No pullup)
uint8_t dht_flg = 0;                        // DHT configured
uint8_t energy_flg = 0;                     // Energy monitor configured
uint8_t i2c_flg = 0;                        // I2C configured
uint8_t spi_flg = 0;                        // SPI configured
uint8_t light_type = 0;                     // Light types
uint8_t ntp_force_sync = 0;                 // Force NTP sync
byte serial_in_byte;                        // Received byte
byte dual_hex_code = 0;                     // Sonoff dual input flag
byte ota_retry_counter = OTA_ATTEMPTS;      // OTA retry counter
byte web_log_index = 1;                     // Index in Web log buffer (should never be 0)
byte reset_web_log_flag = 0;                // Reset web console log
byte devices_present = 0;                   // Max number of devices supported
byte seriallog_level;                       // Current copy of Settings.seriallog_level
byte syslog_level;                          // Current copy of Settings.syslog_level
byte mdns_delayed_start = 0;                // mDNS delayed start
boolean latest_uptime_flag = true;          // Signal latest uptime
boolean pwm_present = false;                // Any PWM channel configured with SetOption15 0
boolean mdns_begun = false;                 // mDNS active
mytmplt my_module;                          // Active copy of Module name and GPIOs (23 x 8 bits)
StateBitfield global_state;                 // Global states (currently Wifi and Mqtt) (8 bits)
char my_version[33];                        // Composed version string
char my_image[33];                          // Code image and/or commit
char my_hostname[33];                       // Composed Wifi hostname
char mqtt_client[33];                       // Composed MQTT Clientname
char mqtt_topic[33];                        // Composed MQTT topic
char serial_in_buffer[INPUT_BUFFER_SIZE];   // Receive buffer
char mqtt_data[MESSZ];                      // MQTT publish buffer and web page ajax buffer
char log_data[LOGSZ];                       // Logging
char web_log[WEB_LOG_SIZE] = {'\0'};        // Web log buffer
String backlog[MAX_BACKLOG];                // Command backlog

/********************************************************************************************/

char* Format(char* output, const char* input, int size)
{
  char *token;
  uint8_t digits = 0;

  if (strstr(input, "%")) {
    strlcpy(output, input, size);
    token = strtok(output, "%");
    if (strstr(input, "%") == input) {
      output[0] = '\0';
    } else {
      token = strtok(NULL, "");
    }
    if (token != NULL) {
      digits = atoi(token);
      if (digits) {
        if (strchr(token, 'd')) {
          snprintf_P(output, size, PSTR("%s%c0%dd"), output, '%', digits);
          snprintf_P(output, size, output, ESP.getChipId() & 0x1fff);       // %04d - short chip ID in dec, like in hostname
        } else {
          snprintf_P(output, size, PSTR("%s%c0%dX"), output, '%', digits);
          snprintf_P(output, size, output, ESP.getChipId());                // %06X - full chip ID in hex
        }
      } else {
        if (strchr(token, 'd')) {
          snprintf_P(output, size, PSTR("%s%d"), output, ESP.getChipId());  // %d - full chip ID in dec
          digits = 8;
        }
      }
    }
  }
  if (!digits) strlcpy(output, input, size);
  return output;
}

char* GetOtaUrl(char *otaurl, size_t otaurl_size)
{
  if (strstr(Settings.ota_url, "%04d") != NULL) {     // OTA url contains placeholder for chip ID
    snprintf(otaurl, otaurl_size, Settings.ota_url, ESP.getChipId() & 0x1fff);
  }
  else if (strstr(Settings.ota_url, "%d") != NULL) {  // OTA url contains placeholder for chip ID
    snprintf_P(otaurl, otaurl_size, Settings.ota_url, ESP.getChipId());
  }
  else {
    snprintf(otaurl, otaurl_size, Settings.ota_url);
  }
  return otaurl;
}

void GetTopic_P(char *stopic, byte prefix, char *topic, const char* subtopic)
{
  /* prefix 0 = Cmnd
     prefix 1 = Stat
     prefix 2 = Tele
  */
  char romram[CMDSZ];
  String fulltopic;

  snprintf_P(romram, sizeof(romram), subtopic);
  if (fallback_topic_flag) {
    fulltopic = FPSTR(kPrefixes[prefix]);
    fulltopic += F("/");
    fulltopic += mqtt_client;
  } else {
    fulltopic = Settings.mqtt_fulltopic;
    if ((0 == prefix) && (-1 == fulltopic.indexOf(F(MQTT_TOKEN_PREFIX)))) {
      fulltopic += F("/" MQTT_TOKEN_PREFIX);  // Need prefix for commands to handle mqtt topic loops
    }
    for (byte i = 0; i < 3; i++) {
      if ('\0' == Settings.mqtt_prefix[i][0]) {
        snprintf_P(Settings.mqtt_prefix[i], sizeof(Settings.mqtt_prefix[i]), kPrefixes[i]);
      }
    }
    fulltopic.replace(F(MQTT_TOKEN_PREFIX), Settings.mqtt_prefix[prefix]);
    fulltopic.replace(F(MQTT_TOKEN_TOPIC), topic);
    fulltopic.replace(F(MQTT_TOKEN_HOSTNAME), my_hostname);
    String token_id = WiFi.macAddress();
    token_id.replace(":", "");
    fulltopic.replace(F(MQTT_TOKEN_ID), token_id);
  }
  fulltopic.replace(F("#"), "");
  fulltopic.replace(F("//"), "/");
  if (!fulltopic.endsWith("/")) fulltopic += "/";
  snprintf_P(stopic, TOPSZ, PSTR("%s%s"), fulltopic.c_str(), romram);
}

char* GetStateText(byte state)
{
  if (state > 3) state = 1;
  return Settings.state_text[state];
}

/********************************************************************************************/

void SetLatchingRelay(power_t lpower, uint8_t state)
{
  // power xx00 - toggle REL1 (Off) and REL3 (Off) - device 1 Off, device 2 Off
  // power xx01 - toggle REL2 (On)  and REL3 (Off) - device 1 On,  device 2 Off
  // power xx10 - toggle REL1 (Off) and REL4 (On)  - device 1 Off, device 2 On
  // power xx11 - toggle REL2 (On)  and REL4 (On)  - device 1 On,  device 2 On

  if (state && !latching_relay_pulse) {  // Set latching relay to power if previous pulse has finished
    latching_power = lpower;
    latching_relay_pulse = 2;            // max 200mS (initiated by stateloop())
  }

  for (byte i = 0; i < devices_present; i++) {
    uint8_t port = (i << 1) + ((latching_power >> i) &1);
    if (pin[GPIO_REL1 +port] < 99) {
      digitalWrite(pin[GPIO_REL1 +port], bitRead(rel_inverted, port) ? !state : state);
    }
  }
}

void SetDevicePower(power_t rpower, int source)
{
  uint8_t state;

  ShowSource(source);

  if (POWER_ALL_ALWAYS_ON == Settings.poweronstate) {  // All on and stay on
    power = (1 << devices_present) -1;
    rpower = power;
  }
  if (Settings.flag.interlock) {     // Allow only one or no relay set
    power_t mask = 1;
    uint8_t count = 0;
    for (byte i = 0; i < devices_present; i++) {
      if (rpower & mask) count++;
      mask <<= 1;
    }
    if (count > 1) {
      power = 0;
      rpower = 0;
    }
  }

  XdrvMailbox.index = rpower;
  XdrvCall(FUNC_SET_POWER);               // Signal power state

  XdrvMailbox.index = rpower;
  XdrvMailbox.payload = source;
  if (XdrvCall(FUNC_SET_DEVICE_POWER)) {  // Set power state and stop if serviced
    // Serviced
  }
  else if ((SONOFF_DUAL == Settings.module) || (CH4 == Settings.module)) {
    Serial.write(0xA0);
    Serial.write(0x04);
    Serial.write(rpower &0xFF);
    Serial.write(0xA1);
    Serial.write('\n');
    Serial.flush();
  }
  else if (EXS_RELAY == Settings.module) {
    SetLatchingRelay(rpower, 1);
  }
  else {
    for (byte i = 0; i < devices_present; i++) {
      state = rpower &1;
      if ((i < MAX_RELAYS) && (pin[GPIO_REL1 +i] < 99)) {
        digitalWrite(pin[GPIO_REL1 +i], bitRead(rel_inverted, i) ? !state : state);
      }
      rpower >>= 1;
    }
  }
}

void SetLedPower(uint8_t state)
{
  if (state) state = 1;
  digitalWrite(pin[GPIO_LED1], (bitRead(led_inverted, 0)) ? !state : state);
}

uint8_t GetFanspeed()
{
  uint8_t fanspeed = 0;

//  if (SONOFF_IFAN02 == Settings.module) {
    /* Fanspeed is controlled by relay 2, 3 and 4 as in Sonoff 4CH.
       000x = 0
       001x = 1
       011x = 2
       101x = 3
    */
    fanspeed = (uint8_t)(power &0xF) >> 1;
    if (fanspeed) { fanspeed = (fanspeed >> 1) +1; }
//  }
  return fanspeed;
}

void SetFanspeed(uint8_t fanspeed)
{
 for (byte i = 0; i < 3; i++) {
    uint8_t state = kIFan02Speed[fanspeed][i];
//    uint8_t state = pgm_read_byte(kIFan02Speed +(speed *3) +i);
    ExecuteCommandPower(i +2, state, SRC_IGNORE);  // Use relay 2, 3 and 4
  }
}

void SetPulseTimer(uint8_t index, uint16_t time)
{
  pulse_timer[index] = (time > 111) ? millis() + (1000 * (time - 100)) : (time > 0) ? millis() + (100 * time) : 0L;
}

uint16_t GetPulseTimer(uint8_t index)
{
  uint16_t result = 0;

  long time = TimePassedSince(pulse_timer[index]);
  if (time < 0) {
    time *= -1;
    result = (time > 11100) ? (time / 1000) + 100 : (time > 0) ? time / 100 : 0;
  }
  return result;
}

/********************************************************************************************/

void MqttDataHandler(char* topic, byte* data, unsigned int data_len)
{
  char *str;

  if (!strcmp(Settings.mqtt_prefix[0],Settings.mqtt_prefix[1])) {
    str = strstr(topic,Settings.mqtt_prefix[0]);
    if ((str == topic) && mqtt_cmnd_publish) {
      if (mqtt_cmnd_publish > 3) {
        mqtt_cmnd_publish -= 3;
      } else {
        mqtt_cmnd_publish = 0;
      }
      return;
    }
  }

  char topicBuf[TOPSZ];
  char dataBuf[data_len+1];
  char command [CMDSZ];
  char stemp1[TOPSZ];
  char *p;
  char *type = NULL;
  byte jsflg = 0;
  byte lines = 1;
  uint8_t grpflg = 0;
//  uint8_t user_append_index = 0;
  uint16_t i = 0;
  uint16_t index;
  uint32_t address;

  ShowFreeMem(PSTR("MqttDataHandler"));

  strlcpy(topicBuf, topic, sizeof(topicBuf));
  for (i = 0; i < data_len; i++) {
    if (!isspace(data[i])) break;
  }
  data_len -= i;
  memcpy(dataBuf, data +i, sizeof(dataBuf));
  dataBuf[sizeof(dataBuf)-1] = 0;

  if (topicBuf[0] != '/') { ShowSource(SRC_MQTT); }

  snprintf_P(log_data, sizeof(log_data), PSTR(D_LOG_RESULT D_RECEIVED_TOPIC " %s, " D_DATA_SIZE " %d, " D_DATA " %s"),
    topicBuf, data_len, dataBuf);
  AddLog(LOG_LEVEL_DEBUG_MORE);
//  if (LOG_LEVEL_DEBUG_MORE <= seriallog_level) Serial.println(dataBuf);

  if (XdrvMqttData(topicBuf, sizeof(topicBuf), dataBuf, sizeof(dataBuf))) return;

  grpflg = (strstr(topicBuf, Settings.mqtt_grptopic) != NULL);
  fallback_topic_flag = (strstr(topicBuf, mqtt_client) != NULL);
  type = strrchr(topicBuf, '/');  // Last part of received topic is always the command (type)

  index = 1;
  if (type != NULL) {
    type++;
    for (i = 0; i < strlen(type); i++) {
      type[i] = toupper(type[i]);
    }
    while (isdigit(type[i-1])) {
      i--;
    }
    if (i < strlen(type)) {
      index = atoi(type +i);
//      user_append_index = 1;
    }
    type[i] = '\0';
  }

  snprintf_P(log_data, sizeof(log_data), PSTR(D_LOG_RESULT D_GROUP " %d, " D_INDEX " %d, " D_COMMAND " %s, " D_DATA " %s"),
    grpflg, index, type, dataBuf);
  AddLog(LOG_LEVEL_DEBUG);

  if (type != NULL) {
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_JSON_COMMAND "\":\"" D_JSON_ERROR "\"}"));
    if (Settings.ledstate &0x02) blinks++;

    if (!strcmp(dataBuf,"?")) data_len = 0;
    int16_t payload = -99;               // No payload
    uint16_t payload16 = 0;
    long payload32 = strtol(dataBuf, &p, 10);
    if (p != dataBuf) {
      payload = (int16_t) payload32;     // -32766 - 32767
      payload16 = (uint16_t) payload32;  // 0 - 65535
    } else {
      payload32 = 0;
    }
    backlog_delay = millis() + (100 * MIN_BACKLOG_DELAY);

    int temp_payload = GetStateNumber(dataBuf);
    if (temp_payload > -1) { payload = temp_payload; }

//    snprintf_P(log_data, sizeof(log_data), PSTR("RSLT: Payload %d, Payload16 %d"), payload, payload16);
//    AddLog(LOG_LEVEL_DEBUG);

    int command_code = GetCommandCode(command, sizeof(command), type, kTasmotaCommands);
    if (-1 == command_code) {
      if (!XdrvCommand(grpflg, type, index, dataBuf, data_len, payload, payload16)) {
        type = NULL;  // Unknown command
      }
    }
    else if (CMND_BACKLOG == command_code) {
      if (data_len) {
        uint8_t bl_pointer = (!backlog_pointer) ? MAX_BACKLOG -1 : backlog_pointer;
        bl_pointer--;
        char *blcommand = strtok(dataBuf, ";");
        while ((blcommand != NULL) && (backlog_index != bl_pointer)) {
          while(true) {
            blcommand = LTrim(blcommand);
            if (!strncasecmp_P(blcommand, PSTR(D_CMND_BACKLOG), strlen(D_CMND_BACKLOG))) {
              blcommand += strlen(D_CMND_BACKLOG);                                  // Skip unnecessary command Backlog
            } else {
              break;
            }
          }
          if (*blcommand != '\0') {
            backlog[backlog_index] = String(blcommand);
            backlog_index++;
            if (backlog_index >= MAX_BACKLOG) backlog_index = 0;
          }
          blcommand = strtok(NULL, ";");
        }
//        snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_SVALUE, command, D_JSON_APPENDED);
        mqtt_data[0] = '\0';
      } else {
        uint8_t blflag = (backlog_pointer == backlog_index);
        backlog_pointer = backlog_index;
        snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_SVALUE, command, blflag ? D_JSON_EMPTY : D_JSON_ABORTED);
      }
    }
    else if (CMND_DELAY == command_code) {
      if ((payload >= MIN_BACKLOG_DELAY) && (payload <= 3600)) {
        backlog_delay = millis() + (100 * payload);
      }
      uint16_t bl_delay = 0;
      long bl_delta = TimePassedSince(backlog_delay);
      if (bl_delta < 0) { bl_delay = (bl_delta *-1) / 100; }
      snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_NVALUE, command, bl_delay);
    }
    else if ((CMND_POWER == command_code) && (index > 0) && (index <= devices_present)) {
      if ((payload < 0) || (payload > 4)) payload = 9;
//      Settings.flag.device_index_enable = user_append_index;
      ExecuteCommandPower(index, payload, SRC_IGNORE);
      fallback_topic_flag = 0;
      return;
    }
    else if ((CMND_FANSPEED == command_code) && (SONOFF_IFAN02 == Settings.module)) {
      if (data_len > 0) {
        if ('-' == dataBuf[0]) {
          payload = (int16_t)GetFanspeed() -1;
          if (payload < 0) { payload = 3; }
        }
        else if ('+' == dataBuf[0]) {
          payload = GetFanspeed() +1;
          if (payload > 3) { payload = 0; }
        }
      }
      if ((payload >= 0) && (payload <= 3) && (payload != GetFanspeed())) {
        SetFanspeed(payload);
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_NVALUE, command, GetFanspeed());
    }
    else if (CMND_STATUS == command_code) {
      if ((payload < 0) || (payload > MAX_STATUS)) payload = 99;
      PublishStatus(payload);
      fallback_topic_flag = 0;
      return;
    }
    else if (CMND_STATE == command_code) {
      mqtt_data[0] = '\0';
      MqttShowState();
    }
    else if (CMND_SLEEP == command_code) {
      if ((payload >= 0) && (payload < 251)) {
        Settings.sleep = payload;
        sleep = payload;
        WiFiSetSleepMode();
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_NVALUE_UNIT_NVALUE_UNIT, command, sleep, (Settings.flag.value_units) ? " " D_UNIT_MILLISECOND : "", Settings.sleep, (Settings.flag.value_units) ? " " D_UNIT_MILLISECOND : "");
    }
    else if ((CMND_UPGRADE == command_code) || (CMND_UPLOAD == command_code)) {
      // Check if the payload is numerically 1, and had no trailing chars.
      //   e.g. "1foo" or "1.2.3" could fool us.
      // Check if the version we have been asked to upgrade to is higher than our current version.
      //   We also need at least 3 chars to make a valid version number string.
      if (((1 == data_len) && (1 == payload)) || ((data_len >= 3) && NewerVersion(dataBuf))) {
        ota_state_flag = 3;
        snprintf_P(mqtt_data, sizeof(mqtt_data), "{\"%s\":\"" D_JSON_VERSION " %s " D_JSON_FROM " %s\"}", command, my_version, GetOtaUrl(stemp1, sizeof(stemp1)));
      } else {
        snprintf_P(mqtt_data, sizeof(mqtt_data), "{\"%s\":\"" D_JSON_ONE_OR_GT "\"}", command, my_version);
      }
    }
    else if (CMND_OTAURL == command_code) {
      if ((data_len > 0) && (data_len < sizeof(Settings.ota_url))) {
        strlcpy(Settings.ota_url, (SC_DEFAULT == Shortcut(dataBuf)) ? OTA_URL : dataBuf, sizeof(Settings.ota_url));
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_SVALUE, command, Settings.ota_url);
    }
    else if (CMND_SERIALLOG == command_code) {
      if ((payload >= LOG_LEVEL_NONE) && (payload <= LOG_LEVEL_ALL)) {
        Settings.flag.mqtt_serial = 0;
        SetSeriallog(payload);
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_NVALUE_ACTIVE_NVALUE, command, Settings.seriallog_level, seriallog_level);
    }
    else if (CMND_RESTART == command_code) {
      switch (payload) {
      case 1:
        restart_flag = 2;
        snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_SVALUE, command, D_JSON_RESTARTING);
        break;
      case 99:
        AddLog_P(LOG_LEVEL_INFO, PSTR(D_LOG_APPLICATION D_RESTARTING));
        EspRestart();
        break;
      default:
        snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_SVALUE, command, D_JSON_ONE_TO_RESTART);
      }
    }
    else if ((CMND_POWERONSTATE == command_code) && (Settings.module != MOTOR)) {
      /* 0 = Keep relays off after power on
       * 1 = Turn relays on after power on, if PulseTime set wait for PulseTime seconds, and turn relays off
       * 2 = Toggle relays after power on
       * 3 = Set relays to last saved state after power on
       * 4 = Turn relays on and disable any relay control (used for Sonoff Pow to always measure power)
       * 5 = Keep relays off after power on, if PulseTime set wait for PulseTime seconds, and turn relays on
       */
      if ((payload >= POWER_ALL_OFF) && (payload <= POWER_ALL_OFF_PULSETIME_ON)) {
        Settings.poweronstate = payload;
        if (POWER_ALL_ALWAYS_ON == Settings.poweronstate) {
          for (byte i = 1; i <= devices_present; i++) {
            ExecuteCommandPower(i, POWER_ON, SRC_IGNORE);
          }
        }
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_NVALUE, command, Settings.poweronstate);
    }
    else if ((CMND_PULSETIME == command_code) && (index > 0) && (index <= MAX_PULSETIMERS)) {
      if (data_len > 0) {
        Settings.pulse_timer[index -1] = payload16;  // 0 - 65535
        SetPulseTimer(index -1, payload16);
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_INDEX_NVALUE_ACTIVE_NVALUE, command, index, Settings.pulse_timer[index -1], GetPulseTimer(index -1));
    }
    else if (CMND_BLINKTIME == command_code) {
      if ((payload > 1) && (payload <= 3600)) {
        Settings.blinktime = payload;
        if (blink_timer > 0) { blink_timer = millis() + (100 * payload); }
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_NVALUE, command, Settings.blinktime);
    }
    else if (CMND_BLINKCOUNT == command_code) {
      if (data_len > 0) {
        Settings.blinkcount = payload16;  // 0 - 65535
        if (blink_counter) blink_counter = Settings.blinkcount *2;
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_NVALUE, command, Settings.blinkcount);
    }
    else if (CMND_SAVEDATA == command_code) {
      if ((payload >= 0) && (payload <= 3600)) {
        Settings.save_data = payload;
        save_data_counter = Settings.save_data;
      }
      SettingsSaveAll();
      if (Settings.save_data > 1) {
        snprintf_P(stemp1, sizeof(stemp1), PSTR(D_JSON_EVERY " %d " D_UNIT_SECOND), Settings.save_data);
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_SVALUE, command, (Settings.save_data > 1) ? stemp1 : GetStateText(Settings.save_data));
    }
    else if ((CMND_SENSOR == command_code) || (CMND_DRIVER == command_code)) {
      XdrvMailbox.index = index;
      XdrvMailbox.data_len = data_len;
      XdrvMailbox.payload16 = payload16;
      XdrvMailbox.payload = payload;
      XdrvMailbox.grpflg = grpflg;
      XdrvMailbox.topic = command;
      XdrvMailbox.data = dataBuf;
      if (CMND_SENSOR == command_code) {
        XsnsCall(FUNC_COMMAND);
      } else {
        XdrvCall(FUNC_COMMAND);
      }
    }
    else if ((CMND_SETOPTION == command_code) && (index < 82)) {
      byte ptype;
      byte pindex;
      if (index <= 31) {         // SetOption0 .. 31 = Settings.flag
        ptype = 0;
        pindex = index;          // 0 .. 31
      }
      else if (index <= 49) {    // SetOption32 .. 49 = Settings.param
        ptype = 2;
        pindex = index -32;      // 0 .. 17 (= PARAM8_SIZE -1)
      }
      else {                     // SetOption50 .. 81 = Settings.flag3
        ptype = 1;
        pindex = index -50;      // 0 .. 31
      }
      if (payload >= 0) {
        if (0 == ptype) {        // SetOption0 .. 31
          if (payload <= 1) {
            switch (pindex) {
              case 5:            // mqtt_power_retain (CMND_POWERRETAIN)
              case 6:            // mqtt_button_retain (CMND_BUTTONRETAIN)
              case 7:            // mqtt_switch_retain (CMND_SWITCHRETAIN)
              case 9:            // mqtt_sensor_retain (CMND_SENSORRETAIN)
              case 22:           // mqtt_serial (SerialSend and SerialLog)
              case 23:           // mqtt_serial_raw (SerialSend)
              case 25:           // knx_enabled (Web config)
              case 27:           // knx_enable_enhancement (Web config)
                ptype = 99;      // Command Error
                break;           // Ignore command SetOption
              case 3:            // mqtt
              case 15:           // pwm_control
                restart_flag = 2;
              default:
                bitWrite(Settings.flag.data, pindex, payload);
            }
            if (12 == pindex) {  // stop_flash_rotate
              stop_flash_rotate = payload;
              SettingsSave(2);
            }
#ifdef USE_HOME_ASSISTANT
            if ((19 == pindex) || (30 == pindex)) {
              HAssDiscovery(1);  // hass_discovery or hass_light
            }
#endif  // USE_HOME_ASSISTANT
          }
        }
        else if (1 == ptype) {   // SetOption50 .. 81
          if (payload <= 1) {
            bitWrite(Settings.flag3.data, pindex, payload);
          }
        }
        else {                   // SetOption32 .. 49
          uint8_t param_low = 0;
          uint8_t param_high = 255;
          switch (pindex) {
            case P_HOLD_TIME:
            case P_MAX_POWER_RETRY:
              param_low = 1;
              param_high = 250;
              break;
          }
          if ((payload >= param_low) && (payload <= param_high)) {
            Settings.param[pindex] = payload;
          }
        }
      }
      if (ptype < 99) {
        if (2 == ptype) snprintf_P(stemp1, sizeof(stemp1), PSTR("%d"), Settings.param[pindex]);
        snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_INDEX_SVALUE, command, index, (2 == ptype) ? stemp1 : (1 == ptype) ? GetStateText(bitRead(Settings.flag3.data, pindex)) : GetStateText(bitRead(Settings.flag.data, pindex)));
      }
    }
    else if ((CMND_SETSENSOR == command_code) && (index < MAX_XSNS_DRIVERS)) {
      if ((payload >= 0) && XsnsPresent(index)) {
        bitWrite(Settings.sensors[index / 32], index % 32, payload &1);
        if (1 == payload) { restart_flag = 2; }  // To safely re-enable a sensor currently most sensor need to follow complete restart init cycle
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_XVALUE, command, XsnsGetSensors().c_str());
    }
    else if (CMND_TEMPERATURE_RESOLUTION == command_code) {
      if ((payload >= 0) && (payload <= 3)) {
        Settings.flag2.temperature_resolution = payload;
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_NVALUE, command, Settings.flag2.temperature_resolution);
    }
    else if (CMND_HUMIDITY_RESOLUTION == command_code) {
      if ((payload >= 0) && (payload <= 3)) {
        Settings.flag2.humidity_resolution = payload;
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_NVALUE, command, Settings.flag2.humidity_resolution);
    }
    else if (CMND_PRESSURE_RESOLUTION == command_code) {
      if ((payload >= 0) && (payload <= 3)) {
        Settings.flag2.pressure_resolution = payload;
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_NVALUE, command, Settings.flag2.pressure_resolution);
    }
    else if (CMND_POWER_RESOLUTION == command_code) {
      if ((payload >= 0) && (payload <= 3)) {
        Settings.flag2.wattage_resolution = payload;
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_NVALUE, command, Settings.flag2.wattage_resolution);
    }
    else if (CMND_VOLTAGE_RESOLUTION == command_code) {
      if ((payload >= 0) && (payload <= 3)) {
        Settings.flag2.voltage_resolution = payload;
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_NVALUE, command, Settings.flag2.voltage_resolution);
    }
    else if (CMND_FREQUENCY_RESOLUTION == command_code) {
      if ((payload >= 0) && (payload <= 3)) {
        Settings.flag2.frequency_resolution = payload;
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_NVALUE, command, Settings.flag2.frequency_resolution);
    }
    else if (CMND_CURRENT_RESOLUTION == command_code) {
      if ((payload >= 0) && (payload <= 3)) {
        Settings.flag2.current_resolution = payload;
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_NVALUE, command, Settings.flag2.current_resolution);
    }
    else if (CMND_ENERGY_RESOLUTION == command_code) {
      if ((payload >= 0) && (payload <= 5)) {
        Settings.flag2.energy_resolution = payload;
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_NVALUE, command, Settings.flag2.energy_resolution);
    }
    else if (CMND_WEIGHT_RESOLUTION == command_code) {
      if ((payload >= 0) && (payload <= 3)) {
        Settings.flag2.weight_resolution = payload;
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_NVALUE, command, Settings.flag2.weight_resolution);
    }
    else if (CMND_MODULE == command_code) {
      if ((payload > 0) && (payload <= MAXMODULE)) {
        payload--;
        Settings.last_module = Settings.module;
        Settings.module = payload;
        if (Settings.last_module != payload) {
          for (byte i = 0; i < MAX_GPIO_PIN; i++) {
            Settings.my_gp.io[i] = 0;
          }
        }
        restart_flag = 2;
      }
      snprintf_P(stemp1, sizeof(stemp1), kModules[Settings.module].name);
      snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_NVALUE_SVALUE, command, Settings.module +1, stemp1);
    }
    else if (CMND_MODULES == command_code) {
      for (byte i = 0; i < MAXMODULE; i++) {
        if (!jsflg) {
          snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_MODULES "%d\":["), lines);
        } else {
          snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s,"), mqtt_data);
        }
        jsflg = 1;
        snprintf_P(stemp1, sizeof(stemp1), kModules[i].name);
        snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s\"%d (%s)\""), mqtt_data, i +1, stemp1);
        if ((strlen(mqtt_data) > (LOGSZ - TOPSZ)) || (i == MAXMODULE -1)) {
          snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s]}"), mqtt_data);
          MqttPublishPrefixTopic_P(RESULT_OR_STAT, type);
          jsflg = 0;
          lines++;
        }
      }
      mqtt_data[0] = '\0';
    }
    else if ((CMND_GPIO == command_code) && (index < MAX_GPIO_PIN)) {
      mytmplt cmodule;
      memcpy_P(&cmodule, &kModules[Settings.module], sizeof(cmodule));
      if ((GPIO_USER == ValidGPIO(index, cmodule.gp.io[index])) && (payload >= 0) && (payload < GPIO_SENSOR_END)) {
        bool present = false;
        for (byte i = 0; i < sizeof(kGpioNiceList); i++) {
          uint8_t midx = pgm_read_byte(kGpioNiceList + i);
          if (midx == payload) { present = true; }
        }
        if (present) {
          for (byte i = 0; i < MAX_GPIO_PIN; i++) {
            if ((GPIO_USER == ValidGPIO(i, cmodule.gp.io[i])) && (Settings.my_gp.io[i] == payload)) {
              Settings.my_gp.io[i] = 0;
            }
          }
          Settings.my_gp.io[index] = payload;
          restart_flag = 2;
        }
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{"));
      for (byte i = 0; i < MAX_GPIO_PIN; i++) {
        if (GPIO_USER == ValidGPIO(i, cmodule.gp.io[i])) {
          if (jsflg) snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s,"), mqtt_data);
          jsflg = 1;
          snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s\"" D_CMND_GPIO "%d\":\"%d (%s)\""),
            mqtt_data, i, Settings.my_gp.io[i], GetTextIndexed(stemp1, sizeof(stemp1), Settings.my_gp.io[i], kSensorNames));
        }
      }
      if (jsflg) {
        snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s}"), mqtt_data);
      } else {
        snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_SVALUE, command, D_JSON_NOT_SUPPORTED);
      }
    }
    else if (CMND_GPIOS == command_code) {
      mytmplt cmodule;
      memcpy_P(&cmodule, &kModules[Settings.module], sizeof(cmodule));
      uint8_t midx;
      for (byte i = 0; i < sizeof(kGpioNiceList); i++) {
        midx = pgm_read_byte(kGpioNiceList + i);
        if (!GetUsedInModule(midx, cmodule.gp.io)) {

          if (!jsflg) {
            snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_GPIOS "%d\":["), lines);
          } else {
            snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s,"), mqtt_data);
          }
          jsflg = 1;
          snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s\"%d (%s)\""), mqtt_data, midx, GetTextIndexed(stemp1, sizeof(stemp1), midx, kSensorNames));
          if ((strlen(mqtt_data) > (LOGSZ - TOPSZ)) || (i == sizeof(kGpioNiceList) -1)) {
            snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s]}"), mqtt_data);
            MqttPublishPrefixTopic_P(RESULT_OR_STAT, type);
            jsflg = 0;
            lines++;
          }
        }
      }

      mqtt_data[0] = '\0';
    }
    else if ((CMND_PWM == command_code) && pwm_present && (index > 0) && (index <= MAX_PWMS)) {
      if ((payload >= 0) && (payload <= Settings.pwm_range) && (pin[GPIO_PWM1 + index -1] < 99)) {
        Settings.pwm_value[index -1] = payload;
        analogWrite(pin[GPIO_PWM1 + index -1], bitRead(pwm_inverted, index -1) ? Settings.pwm_range - payload : payload);
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{"));
      MqttShowPWMState();  // Render the PWM status to MQTT
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s}"), mqtt_data);
    }
    else if (CMND_PWMFREQUENCY == command_code) {
      if ((1 == payload) || ((payload >= PWM_MIN) && (payload <= PWM_MAX))) {
        Settings.pwm_frequency = (1 == payload) ? PWM_FREQ : payload;
        analogWriteFreq(Settings.pwm_frequency);   // Default is 1000 (core_esp8266_wiring_pwm.c)
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_NVALUE, command, Settings.pwm_frequency);
    }
    else if (CMND_PWMRANGE == command_code) {
      if ((1 == payload) || ((payload > 254) && (payload < 1024))) {
        Settings.pwm_range = (1 == payload) ? PWM_RANGE : payload;
        for (byte i = 0; i < MAX_PWMS; i++) {
          if (Settings.pwm_value[i] > Settings.pwm_range) {
            Settings.pwm_value[i] = Settings.pwm_range;
          }
        }
        analogWriteRange(Settings.pwm_range);      // Default is 1023 (Arduino.h)
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_NVALUE, command, Settings.pwm_range);
    }
    else if ((CMND_COUNTER == command_code) && (index > 0) && (index <= MAX_COUNTERS)) {
      if ((data_len > 0) && (pin[GPIO_CNTR1 + index -1] < 99)) {
        if ((dataBuf[0] == '-') || (dataBuf[0] == '+')) {
          RtcSettings.pulse_counter[index -1] += payload32;
          Settings.pulse_counter[index -1] += payload32;
        } else {
          RtcSettings.pulse_counter[index -1] = payload32;
          Settings.pulse_counter[index -1] = payload32;
        }
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_INDEX_LVALUE, command, index, RtcSettings.pulse_counter[index -1]);
    }
    else if ((CMND_COUNTERTYPE == command_code) && (index > 0) && (index <= MAX_COUNTERS)) {
      if ((payload >= 0) && (payload <= 1) && (pin[GPIO_CNTR1 + index -1] < 99)) {
        bitWrite(Settings.pulse_counter_type, index -1, payload &1);
        RtcSettings.pulse_counter[index -1] = 0;
        Settings.pulse_counter[index -1] = 0;
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_INDEX_NVALUE, command, index, bitRead(Settings.pulse_counter_type, index -1));
    }
    else if (CMND_COUNTERDEBOUNCE == command_code) {
      if ((data_len > 0) && (payload16 < 32001)) {
        Settings.pulse_counter_debounce = payload16;
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_NVALUE, command, Settings.pulse_counter_debounce);
    }
    else if (CMND_BUTTONDEBOUNCE == command_code) {
      if ((payload > 39) && (payload < 1001)) {
        Settings.button_debounce = payload;
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_NVALUE, command, Settings.button_debounce);
    }
    else if (CMND_SWITCHDEBOUNCE == command_code) {
      if ((payload > 39) && (payload < 1001)) {
        Settings.switch_debounce = payload;
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_NVALUE, command, Settings.switch_debounce);
    }
    else if (CMND_BAUDRATE == command_code) {
      if (payload32 > 0) {
        payload32 /= 1200;  // Make it a valid baudrate
        baudrate = (1 == payload) ? APP_BAUDRATE : payload32 * 1200;
        SetSerialBaudrate(baudrate);
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_NVALUE, command, Settings.baudrate * 1200);
    }
    else if ((CMND_SERIALSEND == command_code) && (index > 0) && (index <= 5)) {
      SetSeriallog(LOG_LEVEL_NONE);
      Settings.flag.mqtt_serial = 1;
      Settings.flag.mqtt_serial_raw = (index > 3) ? 1 : 0;
      if (data_len > 0) {
        if (1 == index) {
          Serial.printf("%s\n", dataBuf);                    // "Hello Tiger\n"
        }
        else if (2 == index || 4 == index) {
          for (int i = 0; i < data_len; i++) {
            Serial.write(dataBuf[i]);                        // "Hello Tiger" or "A0"
          }
        }
        else if (3 == index) {
          uint16_t dat_len = data_len;
          Serial.printf("%s", Unescape(dataBuf, &dat_len));  // "Hello\f"
        }
        else if (5 == index) {
          SerialSendRaw(RemoveSpace(dataBuf));  // "AA004566"
        }
        snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_SVALUE, command, D_JSON_DONE);
      }
    }
    else if (CMND_SERIALDELIMITER == command_code) {
      if ((data_len > 0) && (payload < 256)) {
        if (payload > 0) {
          Settings.serial_delimiter = payload;
        } else {
          uint16_t dat_len = data_len;
          Unescape(dataBuf, &dat_len);
          Settings.serial_delimiter = dataBuf[0];
        }
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_NVALUE, command, Settings.serial_delimiter);
    }
    else if (CMND_SYSLOG == command_code) {
      if ((payload >= LOG_LEVEL_NONE) && (payload <= LOG_LEVEL_ALL)) {
        Settings.syslog_level = payload;
        syslog_level = payload;
        syslog_timer = 0;
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_NVALUE_ACTIVE_NVALUE, command, Settings.syslog_level, syslog_level);
    }
    else if (CMND_LOGHOST == command_code) {
      if ((data_len > 0) && (data_len < sizeof(Settings.syslog_host))) {
        strlcpy(Settings.syslog_host, (SC_DEFAULT == Shortcut(dataBuf)) ? SYS_LOG_HOST : dataBuf, sizeof(Settings.syslog_host));
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_SVALUE, command, Settings.syslog_host);
    }
    else if (CMND_LOGPORT == command_code) {
      if (payload16 > 0) {
        Settings.syslog_port = (1 == payload16) ? SYS_LOG_PORT : payload16;
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_NVALUE, command, Settings.syslog_port);
    }
    else if ((CMND_IPADDRESS == command_code) && (index > 0) && (index <= 4)) {
      if (ParseIp(&address, dataBuf)) {
        Settings.ip_address[index -1] = address;
//        restart_flag = 2;
      }
      snprintf_P(stemp1, sizeof(stemp1), PSTR(" (%s)"), WiFi.localIP().toString().c_str());
      snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_INDEX_SVALUE_SVALUE, command, index, IPAddress(Settings.ip_address[index -1]).toString().c_str(), (1 == index) ? stemp1:"");
    }
    else if ((CMND_NTPSERVER == command_code) && (index > 0) && (index <= 3)) {
      if ((data_len > 0) && (data_len < sizeof(Settings.ntp_server[0]))) {
        strlcpy(Settings.ntp_server[index -1], (SC_CLEAR == Shortcut(dataBuf)) ? "" : (SC_DEFAULT == Shortcut(dataBuf)) ? (1==index)?NTP_SERVER1:(2==index)?NTP_SERVER2:NTP_SERVER3 : dataBuf, sizeof(Settings.ntp_server[0]));
        for (i = 0; i < strlen(Settings.ntp_server[index -1]); i++) {
          if (Settings.ntp_server[index -1][i] == ',') Settings.ntp_server[index -1][i] = '.';
        }
//        restart_flag = 2;  // Issue #3890
        ntp_force_sync = 1;
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_INDEX_SVALUE, command, index, Settings.ntp_server[index -1]);
    }
    else if (CMND_AP == command_code) {
      if ((payload >= 0) && (payload <= 2)) {
        switch (payload) {
        case 0:  // Toggle
          Settings.sta_active ^= 1;
          break;
        case 1:  // AP1
        case 2:  // AP2
          Settings.sta_active = payload -1;
        }
        restart_flag = 2;
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_NVALUE_SVALUE, command, Settings.sta_active +1, Settings.sta_ssid[Settings.sta_active]);
    }
    else if ((CMND_SSID == command_code) && (index > 0) && (index <= 2)) {
      if ((data_len > 0) && (data_len < sizeof(Settings.sta_ssid[0]))) {
        strlcpy(Settings.sta_ssid[index -1], (SC_CLEAR == Shortcut(dataBuf)) ? "" : (SC_DEFAULT == Shortcut(dataBuf)) ? (1 == index) ? STA_SSID1 : STA_SSID2 : dataBuf, sizeof(Settings.sta_ssid[0]));
        Settings.sta_active = index -1;
        restart_flag = 2;
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_INDEX_SVALUE, command, index, Settings.sta_ssid[index -1]);
    }
    else if ((CMND_PASSWORD == command_code) && (index > 0) && (index <= 2)) {
      if ((data_len > 0) && (data_len < sizeof(Settings.sta_pwd[0]))) {
        strlcpy(Settings.sta_pwd[index -1], (SC_CLEAR == Shortcut(dataBuf)) ? "" : (SC_DEFAULT == Shortcut(dataBuf)) ? (1 == index) ? STA_PASS1 : STA_PASS2 : dataBuf, sizeof(Settings.sta_pwd[0]));
        Settings.sta_active = index -1;
        restart_flag = 2;
        snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_INDEX_SVALUE, command, index, Settings.sta_pwd[index -1]);
      } else {
        snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_INDEX_ASTERIX, command, index);
      }
    }
    else if ((CMND_HOSTNAME == command_code) && !grpflg) {
      if ((data_len > 0) && (data_len < sizeof(Settings.hostname))) {
        strlcpy(Settings.hostname, (SC_DEFAULT == Shortcut(dataBuf)) ? WIFI_HOSTNAME : dataBuf, sizeof(Settings.hostname));
        if (strstr(Settings.hostname,"%")) {
          strlcpy(Settings.hostname, WIFI_HOSTNAME, sizeof(Settings.hostname));
        }
        restart_flag = 2;
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_SVALUE, command, Settings.hostname);
    }
    else if (CMND_WIFICONFIG == command_code) {
      if ((payload >= WIFI_RESTART) && (payload < MAX_WIFI_OPTION)) {
        Settings.sta_config = payload;
        wifi_state_flag = Settings.sta_config;
        snprintf_P(stemp1, sizeof(stemp1), kWifiConfig[Settings.sta_config]);
        snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_WIFICONFIG "\":\"%s " D_JSON_SELECTED "\"}"), stemp1);
        if (WifiState() > WIFI_RESTART) {
//          snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s after restart"), mqtt_data);
          restart_flag = 2;
        }
      } else {
        snprintf_P(stemp1, sizeof(stemp1), kWifiConfig[Settings.sta_config]);
        snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_NVALUE_SVALUE, command, Settings.sta_config, stemp1);
      }
    }
    else if ((CMND_FRIENDLYNAME == command_code) && (index > 0) && (index <= MAX_FRIENDLYNAMES)) {
      if ((data_len > 0) && (data_len < sizeof(Settings.friendlyname[0]))) {
        if (1 == index) {
          snprintf_P(stemp1, sizeof(stemp1), PSTR(FRIENDLY_NAME));
        } else {
          snprintf_P(stemp1, sizeof(stemp1), PSTR(FRIENDLY_NAME "%d"), index);
        }
        strlcpy(Settings.friendlyname[index -1], (SC_DEFAULT == Shortcut(dataBuf)) ? stemp1 : dataBuf, sizeof(Settings.friendlyname[index -1]));
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_INDEX_SVALUE, command, index, Settings.friendlyname[index -1]);
    }
    else if ((CMND_SWITCHMODE == command_code) && (index > 0) && (index <= MAX_SWITCHES)) {
      if ((payload >= 0) && (payload < MAX_SWITCH_OPTION)) {
        Settings.switchmode[index -1] = payload;
        GpioSwitchPinMode(index -1);
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_INDEX_NVALUE, command, index, Settings.switchmode[index-1]);
    }
    else if (CMND_TELEPERIOD == command_code) {
      if ((payload >= 0) && (payload < 3601)) {
        Settings.tele_period = (1 == payload) ? TELE_PERIOD : payload;
        if ((Settings.tele_period > 0) && (Settings.tele_period < 10)) Settings.tele_period = 10;   // Do not allow periods < 10 seconds
        tele_period = Settings.tele_period;
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_NVALUE_UNIT, command, Settings.tele_period, (Settings.flag.value_units) ? " " D_UNIT_SECOND : "");
    }
    else if (CMND_RESET == command_code) {
      switch (payload) {
      case 1:
        restart_flag = 211;
        snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_SVALUE, command , D_JSON_RESET_AND_RESTARTING);
        break;
      case 2:
      case 3:
      case 4:
      case 5:
        restart_flag = 210 + payload;
        snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_RESET "\":\"" D_JSON_ERASE ", " D_JSON_RESET_AND_RESTARTING "\"}"));
        break;
      default:
        snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_SVALUE, command, D_JSON_ONE_TO_RESET);
      }
    }
    else if (CMND_TIMEZONE == command_code) {
      if ((data_len > 0) && (payload >= -13)) {
        Settings.timezone = payload;
        Settings.timezone_minutes = 0;
        if (payload < 15) {
          p = strtok (dataBuf, ":");
          if (p) {
            p = strtok (NULL, ":");
            if (p) {
              Settings.timezone_minutes = strtol(p, NULL, 10);
              if (Settings.timezone_minutes > 59) { Settings.timezone_minutes = 59; }
            }
          }
        } else {
          Settings.timezone = 99;
        }
        ntp_force_sync = 1;
      }
      if (99 == Settings.timezone) {
        snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_NVALUE, command, Settings.timezone);
      } else {
        snprintf_P(stemp1, sizeof(stemp1), PSTR("%+03d:%02d"), Settings.timezone, Settings.timezone_minutes);
        snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_SVALUE, command, stemp1);
      }
    }
    else if ((CMND_TIMESTD == command_code) || (CMND_TIMEDST == command_code)) {
      // TimeStd 0/1, 0/1/2/3/4, 1..12, 1..7, 0..23, +/-780
      uint8_t ts = 0;
      if (CMND_TIMEDST == command_code) { ts = 1; }
      if (data_len > 0) {
        if (strstr(dataBuf, ",")) {              // Process parameter entry
          uint8_t tpos = 0;                      // Parameter index
          int value = 0;
          p = dataBuf;                           // Parameters like "1, 2,3 , 4 ,5, -120" or ",,,,,+240"
          char *q = p;                           // Value entered flag
          while (p && (tpos < 7)) {
            if (p > q) {                         // Any value entered
              if (1 == tpos) { Settings.tflag[ts].hemis = value &1; }
              if (2 == tpos) { Settings.tflag[ts].week = (value < 0) ? 0 : (value > 4) ? 4 : value; }
              if (3 == tpos) { Settings.tflag[ts].month = (value < 1) ? 1 : (value > 12) ? 12 : value; }
              if (4 == tpos) { Settings.tflag[ts].dow = (value < 1) ? 1 : (value > 7) ? 7 : value; }
              if (5 == tpos) { Settings.tflag[ts].hour = (value < 0) ? 0 : (value > 23) ? 23 : value; }
              if (6 == tpos) { Settings.toffset[ts] = (value < -900) ? -900 : (value > 900) ? 900 : value; }
            }
            p = LTrim(p);                        // Skip spaces
            if (tpos && (*p == ',')) { p++; }    // Skip separator
            p = LTrim(p);                        // Skip spaces
            q = p;                               // Reset any value entered flag
            value = strtol(p, &p, 10);
            tpos++;                              // Next parameter
          }
          ntp_force_sync = 1;
        } else {
          if (0 == payload) {
            if (0 == ts) {
              SettingsResetStd();
            } else {
              SettingsResetDst();
            }
          }
          ntp_force_sync = 1;
        }
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"%s\":{\"Hemisphere\":%d,\"Week\":%d,\"Month\":%d,\"Day\":%d,\"Hour\":%d,\"Offset\":%d}}"),
        command, Settings.tflag[ts].hemis, Settings.tflag[ts].week, Settings.tflag[ts].month, Settings.tflag[ts].dow, Settings.tflag[ts].hour, Settings.toffset[ts]);
    }
    else if (CMND_ALTITUDE == command_code) {
      if ((data_len > 0) && ((payload >= -30000) && (payload <= 30000))) {
        Settings.altitude = payload;
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_NVALUE, command, Settings.altitude);
    }
    else if (CMND_LEDPOWER == command_code) {
      if ((payload >= 0) && (payload <= 2)) {
        Settings.ledstate &= 8;
        switch (payload) {
        case 0: // Off
        case 1: // On
          Settings.ledstate = payload << 3;
          break;
        case 2: // Toggle
          Settings.ledstate ^= 8;
          break;
        }
        blinks = 0;
        SetLedPower(Settings.ledstate &8);
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_SVALUE, command, GetStateText(bitRead(Settings.ledstate, 3)));
    }
    else if (CMND_LEDSTATE == command_code) {
      if ((payload >= 0) && (payload < MAX_LED_OPTION)) {
        Settings.ledstate = payload;
        if (!Settings.ledstate) SetLedPower(0);
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_COMMAND_NVALUE, command, Settings.ledstate);
    }
#ifdef USE_I2C
    else if ((CMND_I2CSCAN == command_code) && i2c_flg) {
      I2cScan(mqtt_data, sizeof(mqtt_data));
    }
#endif  // USE_I2C
    else type = NULL;  // Unknown command
  }
  if (type == NULL) {
    blinks = 201;
    snprintf_P(topicBuf, sizeof(topicBuf), PSTR(D_JSON_COMMAND));
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_JSON_COMMAND "\":\"" D_JSON_UNKNOWN "\"}"));
    type = (char*)topicBuf;
  }
  if (mqtt_data[0] != '\0') MqttPublishPrefixTopic_P(RESULT_OR_STAT, type);
  fallback_topic_flag = 0;
}

/********************************************************************************************/

boolean SendKey(byte key, byte device, byte state)
{
// key 0 = button_topic
// key 1 = switch_topic
// state 0 = off
// state 1 = on
// state 2 = toggle
// state 3 = hold
// state 9 = clear retain flag

  char stopic[TOPSZ];
  char scommand[CMDSZ];
  char key_topic[sizeof(Settings.button_topic)];
  boolean result = false;

  char *tmp = (key) ? Settings.switch_topic : Settings.button_topic;
  Format(key_topic, tmp, sizeof(key_topic));
  if (Settings.flag.mqtt_enabled && MqttIsConnected() && (strlen(key_topic) != 0) && strcmp(key_topic, "0")) {
    if (!key && (device > devices_present)) device = 1;                                             // Only allow number of buttons up to number of devices
    GetTopic_P(stopic, CMND, key_topic, GetPowerDevice(scommand, device, sizeof(scommand), key));   // cmnd/switchtopic/POWERx
    if (9 == state) {
      mqtt_data[0] = '\0';
    } else {
      if ((!strcmp(mqtt_topic, key_topic) || !strcmp(Settings.mqtt_grptopic, key_topic)) && (2 == state)) {
        state = ~(power >> (device -1)) &1;
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), GetStateText(state));
    }
#ifdef USE_DOMOTICZ
    if (!(DomoticzSendKey(key, device, state, strlen(mqtt_data)))) {
      MqttPublishDirect(stopic, (key) ? Settings.flag.mqtt_switch_retain : Settings.flag.mqtt_button_retain);
    }
#else
    MqttPublishDirect(stopic, (key) ? Settings.flag.mqtt_switch_retain : Settings.flag.mqtt_button_retain);
#endif  // USE_DOMOTICZ
    result = true;
  } else {
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"%s%d\":{\"State\":%d}}"), (key) ? "Switch" : "Button", device, state);
    result = XdrvRulesProcess();
  }
#ifdef USE_KNX
  KnxSendButtonPower(key, device, state);
#endif  // USE_KNX
  return result;
}

void ExecuteCommandPower(byte device, byte state, int source)
{
// device  = Relay number 1 and up
// state 0 = Relay Off
// state 1 = Relay On (turn off after Settings.pulse_timer * 100 mSec if enabled)
// state 2 = Toggle relay
// state 3 = Blink relay
// state 4 = Stop blinking relay
// state 6 = Relay Off and no publishPowerState
// state 7 = Relay On and no publishPowerState
// state 9 = Show power state

//  ShowSource(source);

  if (SONOFF_IFAN02 == Settings.module) {
    blink_mask &= 1;                 // No blinking on the fan relays
    Settings.flag.interlock = 0;     // No interlock mode as it is already done by the microcontroller
    Settings.pulse_timer[1] = 0;     // No pulsetimers on the fan relays
    Settings.pulse_timer[2] = 0;
    Settings.pulse_timer[3] = 0;
  }

  uint8_t publish_power = 1;
  if ((POWER_OFF_NO_STATE == state) || (POWER_ON_NO_STATE == state)) {
    state &= 1;
    publish_power = 0;
  }
  if ((device < 1) || (device > devices_present)) device = 1;
  if (device <= MAX_PULSETIMERS) { SetPulseTimer(device -1, 0); }
  power_t mask = 1 << (device -1);
  if (state <= POWER_TOGGLE) {
    if ((blink_mask & mask)) {
      blink_mask &= (POWER_MASK ^ mask);  // Clear device mask
      MqttPublishPowerBlinkState(device);
    }
    if (Settings.flag.interlock && !interlock_mutex) {  // Clear all but masked relay
      interlock_mutex = 1;
      for (byte i = 0; i < devices_present; i++) {
        power_t imask = 1 << i;
        if ((power & imask) && (mask != imask)) ExecuteCommandPower(i +1, POWER_OFF, SRC_IGNORE);
      }
      interlock_mutex = 0;
    }
    switch (state) {
    case POWER_OFF: {
      power &= (POWER_MASK ^ mask);
      break; }
    case POWER_ON:
      power |= mask;
      break;
    case POWER_TOGGLE:
      power ^= mask;
    }
    SetDevicePower(power, source);
#ifdef USE_DOMOTICZ
    DomoticzUpdatePowerState(device);
#endif  // USE_DOMOTICZ
#ifdef USE_KNX
    KnxUpdatePowerState(device, power);
#endif  // USE_KNX
    if (device <= MAX_PULSETIMERS) {  // Restart PulseTime if powered On
      SetPulseTimer(device -1, (((POWER_ALL_OFF_PULSETIME_ON == Settings.poweronstate) ? ~power : power) & mask) ? Settings.pulse_timer[device -1] : 0);
    }
  }
  else if (POWER_BLINK == state) {
    if (!(blink_mask & mask)) {
      blink_powersave = (blink_powersave & (POWER_MASK ^ mask)) | (power & mask);  // Save state
      blink_power = (power >> (device -1))&1;  // Prep to Toggle
    }
    blink_timer = millis() + 100;
    blink_counter = ((!Settings.blinkcount) ? 64000 : (Settings.blinkcount *2)) +1;
    blink_mask |= mask;  // Set device mask
    MqttPublishPowerBlinkState(device);
    return;
  }
  else if (POWER_BLINK_STOP == state) {
    byte flag = (blink_mask & mask);
    blink_mask &= (POWER_MASK ^ mask);  // Clear device mask
    MqttPublishPowerBlinkState(device);
    if (flag) ExecuteCommandPower(device, (blink_powersave >> (device -1))&1, SRC_IGNORE);  // Restore state
    return;
  }
  if (publish_power) MqttPublishPowerState(device);
}

void StopAllPowerBlink()
{
  power_t mask;

  for (byte i = 1; i <= devices_present; i++) {
    mask = 1 << (i -1);
    if (blink_mask & mask) {
      blink_mask &= (POWER_MASK ^ mask);  // Clear device mask
      MqttPublishPowerBlinkState(i);
      ExecuteCommandPower(i, (blink_powersave >> (i -1))&1, SRC_IGNORE);  // Restore state
    }
  }
}

void ExecuteCommand(char *cmnd, int source)
{
  char stopic[CMDSZ];
  char svalue[INPUT_BUFFER_SIZE];
  char *start;
  char *token;

  ShowFreeMem(PSTR("ExecuteCommand"));
  ShowSource(source);

  token = strtok(cmnd, " ");
  if (token != NULL) {
    start = strrchr(token, '/');   // Skip possible cmnd/sonoff/ preamble
    if (start) token = start +1;
  }
  snprintf_P(stopic, sizeof(stopic), PSTR("/%s"), (token == NULL) ? "" : token);
  token = strtok(NULL, "");
//  snprintf_P(svalue, sizeof(svalue), (token == NULL) ? "" : token);  // Fails with command FullTopic home/%prefix%/%topic% as it processes %p of %prefix%
  strlcpy(svalue, (token == NULL) ? "" : token, sizeof(svalue));       // Fixed 5.8.0b
  MqttDataHandler(stopic, (byte*)svalue, strlen(svalue));
}

void PublishStatus(uint8_t payload)
{
  uint8_t option = STAT;
  char stemp[MAX_FRIENDLYNAMES * (sizeof(Settings.friendlyname[0]) +MAX_FRIENDLYNAMES)];
  char stemp2[MAX_SWITCHES * 3];

  // Workaround MQTT - TCP/IP stack queueing when SUB_PREFIX = PUB_PREFIX
  if (!strcmp(Settings.mqtt_prefix[0],Settings.mqtt_prefix[1]) && (!payload)) option++;  // TELE

  if ((!Settings.flag.mqtt_enabled) && (6 == payload)) payload = 99;
  if (!energy_flg && (9 == payload)) payload = 99;

  if ((0 == payload) || (99 == payload)) {
    uint8_t maxfn = (devices_present > MAX_FRIENDLYNAMES) ? MAX_FRIENDLYNAMES : (!devices_present) ? 1 : devices_present;
    if (SONOFF_IFAN02 == Settings.module) { maxfn = 1; }
    stemp[0] = '\0';
    for (byte i = 0; i < maxfn; i++) {
      snprintf_P(stemp, sizeof(stemp), PSTR("%s%s\"%s\"" ), stemp, (i > 0 ? "," : ""), Settings.friendlyname[i]);
    }
    stemp2[0] = '\0';
    for (byte i = 0; i < MAX_SWITCHES; i++) {
      snprintf_P(stemp2, sizeof(stemp2), PSTR("%s%s%d" ), stemp2, (i > 0 ? "," : ""), Settings.switchmode[i]);
    }
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_STATUS "\":{\"" D_CMND_MODULE "\":%d,\"" D_CMND_FRIENDLYNAME "\":[%s],\"" D_CMND_TOPIC "\":\"%s\",\"" D_CMND_BUTTONTOPIC "\":\"%s\",\"" D_CMND_POWER "\":%d,\"" D_CMND_POWERONSTATE "\":%d,\"" D_CMND_LEDSTATE "\":%d,\"" D_CMND_SAVEDATA "\":%d,\"" D_JSON_SAVESTATE "\":%d,\"" D_CMND_SWITCHTOPIC "\":\"%s\",\"" D_CMND_SWITCHMODE "\":[%s],\"" D_CMND_BUTTONRETAIN "\":%d,\"" D_CMND_SWITCHRETAIN "\":%d,\"" D_CMND_SENSORRETAIN "\":%d,\"" D_CMND_POWERRETAIN "\":%d}}"),
      Settings.module +1, stemp, mqtt_topic, Settings.button_topic, power, Settings.poweronstate, Settings.ledstate, Settings.save_data, Settings.flag.save_state, Settings.switch_topic, stemp2, Settings.flag.mqtt_button_retain, Settings.flag.mqtt_switch_retain, Settings.flag.mqtt_sensor_retain, Settings.flag.mqtt_power_retain);
    MqttPublishPrefixTopic_P(option, PSTR(D_CMND_STATUS));
  }

  if ((0 == payload) || (1 == payload)) {
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_STATUS D_STATUS1_PARAMETER "\":{\"" D_JSON_BAUDRATE "\":%d,\"" D_CMND_GROUPTOPIC "\":\"%s\",\"" D_CMND_OTAURL "\":\"%s\",\"" D_JSON_RESTARTREASON "\":\"%s\",\"" D_JSON_UPTIME "\":\"%s\",\"" D_JSON_STARTUPUTC "\":\"%s\",\"" D_CMND_SLEEP "\":%d,\"" D_JSON_BOOTCOUNT "\":%d,\"" D_JSON_SAVECOUNT "\":%d,\"" D_JSON_SAVEADDRESS "\":\"%X\"}}"),
      baudrate, Settings.mqtt_grptopic, Settings.ota_url, GetResetReason().c_str(), GetUptime().c_str(), GetDateAndTime(DT_RESTART).c_str(), Settings.sleep, Settings.bootcount, Settings.save_flag, GetSettingsAddress());
    MqttPublishPrefixTopic_P(option, PSTR(D_CMND_STATUS "1"));
  }

  if ((0 == payload) || (2 == payload)) {
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_STATUS D_STATUS2_FIRMWARE "\":{\"" D_JSON_VERSION "\":\"%s%s\",\"" D_JSON_BUILDDATETIME "\":\"%s\",\"" D_JSON_BOOTVERSION "\":%d,\"" D_JSON_COREVERSION "\":\"" ARDUINO_ESP8266_RELEASE "\",\"" D_JSON_SDKVERSION "\":\"%s\"}}"),
      my_version, my_image, GetBuildDateAndTime().c_str(), ESP.getBootVersion(), ESP.getSdkVersion());
    MqttPublishPrefixTopic_P(option, PSTR(D_CMND_STATUS "2"));
  }

  if ((0 == payload) || (3 == payload)) {
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_STATUS D_STATUS3_LOGGING "\":{\"" D_CMND_SERIALLOG "\":%d,\"" D_CMND_WEBLOG "\":%d,\"" D_CMND_SYSLOG "\":%d,\"" D_CMND_LOGHOST "\":\"%s\",\"" D_CMND_LOGPORT "\":%d,\"" D_CMND_SSID "\":[\"%s\",\"%s\"],\"" D_CMND_TELEPERIOD "\":%d,\"" D_CMND_SETOPTION "\":[\"%08X\",\"%08X\",\"%08X\"]}}"),
      Settings.seriallog_level, Settings.weblog_level, Settings.syslog_level, Settings.syslog_host, Settings.syslog_port, Settings.sta_ssid[0], Settings.sta_ssid[1], Settings.tele_period, Settings.flag.data, Settings.flag2.data, Settings.flag3.data);
    MqttPublishPrefixTopic_P(option, PSTR(D_CMND_STATUS "3"));
  }

  if ((0 == payload) || (4 == payload)) {
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_STATUS D_STATUS4_MEMORY "\":{\"" D_JSON_PROGRAMSIZE "\":%d,\"" D_JSON_FREEMEMORY "\":%d,\"" D_JSON_HEAPSIZE "\":%d,\"" D_JSON_PROGRAMFLASHSIZE "\":%d,\"" D_JSON_FLASHSIZE "\":%d,\"" D_JSON_FLASHCHIPID "\":\"%06X\",\"" D_JSON_FLASHMODE "\":%d,\"" D_JSON_FEATURES "\":[\"%08X\",\"%08X\",\"%08X\",\"%08X\",\"%08X\"]}}"),
      ESP.getSketchSize()/1024, ESP.getFreeSketchSpace()/1024, ESP.getFreeHeap()/1024, ESP.getFlashChipSize()/1024, ESP.getFlashChipRealSize()/1024, ESP.getFlashChipId(), ESP.getFlashChipMode(), LANGUAGE_LCID, feature_drv1, feature_drv2, feature_sns1, feature_sns2);
    MqttPublishPrefixTopic_P(option, PSTR(D_CMND_STATUS "4"));
  }

  if ((0 == payload) || (5 == payload)) {
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_STATUS D_STATUS5_NETWORK "\":{\"" D_CMND_HOSTNAME "\":\"%s\",\"" D_CMND_IPADDRESS "\":\"%s\",\"" D_JSON_GATEWAY "\":\"%s\",\"" D_JSON_SUBNETMASK "\":\"%s\",\"" D_JSON_DNSSERVER "\":\"%s\",\"" D_JSON_MAC "\":\"%s\",\"" D_CMND_WEBSERVER "\":%d,\"" D_CMND_WIFICONFIG "\":%d}}"),
      my_hostname, WiFi.localIP().toString().c_str(), IPAddress(Settings.ip_address[1]).toString().c_str(), IPAddress(Settings.ip_address[2]).toString().c_str(), IPAddress(Settings.ip_address[3]).toString().c_str(),
      WiFi.macAddress().c_str(), Settings.webserver, Settings.sta_config);
    MqttPublishPrefixTopic_P(option, PSTR(D_CMND_STATUS "5"));
  }

  if (((0 == payload) || (6 == payload)) && Settings.flag.mqtt_enabled) {
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_STATUS D_STATUS6_MQTT "\":{\"" D_CMND_MQTTHOST "\":\"%s\",\"" D_CMND_MQTTPORT "\":%d,\"" D_CMND_MQTTCLIENT D_JSON_MASK "\":\"%s\",\"" D_CMND_MQTTCLIENT "\":\"%s\",\"" D_CMND_MQTTUSER "\":\"%s\",\"MqttType\":%d,\"MAX_PACKET_SIZE\":%d,\"KEEPALIVE\":%d}}"),
      Settings.mqtt_host, Settings.mqtt_port, Settings.mqtt_client, mqtt_client, Settings.mqtt_user, MqttLibraryType(), MQTT_MAX_PACKET_SIZE, MQTT_KEEPALIVE);
    MqttPublishPrefixTopic_P(option, PSTR(D_CMND_STATUS "6"));
  }

  if ((0 == payload) || (7 == payload)) {
    if (99 == Settings.timezone) {
      snprintf_P(stemp, sizeof(stemp), PSTR("%d" ), Settings.timezone);
    } else {
      snprintf_P(stemp, sizeof(stemp), PSTR("\"%s\"" ), GetTimeZone().c_str());
    }
#if defined(USE_TIMERS) && defined(USE_SUNRISE)
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_STATUS D_STATUS7_TIME "\":{\"" D_JSON_UTC_TIME "\":\"%s\",\"" D_JSON_LOCAL_TIME "\":\"%s\",\"" D_JSON_STARTDST "\":\"%s\",\"" D_JSON_ENDDST "\":\"%s\",\"" D_CMND_TIMEZONE "\":%s,\"" D_JSON_SUNRISE "\":\"%s\",\"" D_JSON_SUNSET "\":\"%s\"}}"),
      GetTime(0).c_str(), GetTime(1).c_str(), GetTime(2).c_str(), GetTime(3).c_str(), stemp, GetSun(0).c_str(), GetSun(1).c_str());
#else
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_STATUS D_STATUS7_TIME "\":{\"" D_JSON_UTC_TIME "\":\"%s\",\"" D_JSON_LOCAL_TIME "\":\"%s\",\"" D_JSON_STARTDST "\":\"%s\",\"" D_JSON_ENDDST "\":\"%s\",\"" D_CMND_TIMEZONE "\":%s}}"),
      GetTime(0).c_str(), GetTime(1).c_str(), GetTime(2).c_str(), GetTime(3).c_str(), stemp);
#endif  // USE_TIMERS and USE_SUNRISE
    MqttPublishPrefixTopic_P(option, PSTR(D_CMND_STATUS "7"));
  }

  if (energy_flg) {
    if ((0 == payload) || (9 == payload)) {
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_STATUS D_STATUS9_MARGIN "\":{\"" D_CMND_POWERDELTA "\":%d,\"" D_CMND_POWERLOW "\":%d,\"" D_CMND_POWERHIGH "\":%d,\"" D_CMND_VOLTAGELOW "\":%d,\"" D_CMND_VOLTAGEHIGH "\":%d,\"" D_CMND_CURRENTLOW "\":%d,\"" D_CMND_CURRENTHIGH "\":%d}}"),
        Settings.energy_power_delta, Settings.energy_min_power, Settings.energy_max_power, Settings.energy_min_voltage, Settings.energy_max_voltage, Settings.energy_min_current, Settings.energy_max_current);
      MqttPublishPrefixTopic_P(option, PSTR(D_CMND_STATUS "9"));
    }
  }

  if ((0 == payload) || (8 == payload) || (10 == payload)) {
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_STATUS D_STATUS10_SENSOR "\":"));
    MqttShowSensor();
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s}"), mqtt_data);
    if (8 == payload) {
      MqttPublishPrefixTopic_P(option, PSTR(D_CMND_STATUS "8"));
    } else {
      MqttPublishPrefixTopic_P(option, PSTR(D_CMND_STATUS "10"));
    }
  }

  if ((0 == payload) || (11 == payload)) {
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_STATUS D_STATUS11_STATUS "\":"));
    MqttShowState();
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s}"), mqtt_data);
    MqttPublishPrefixTopic_P(option, PSTR(D_CMND_STATUS "11"));
  }

}

void MqttShowPWMState()
{
  snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s\"" D_CMND_PWM "\":{"), mqtt_data);
  bool first = true;
  for (byte i = 0; i < MAX_PWMS; i++) {
    if (pin[GPIO_PWM1 + i] < 99) {
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s%s\"" D_CMND_PWM "%d\":%d"), mqtt_data, first ? "" : ",", i+1, Settings.pwm_value[i]);
      first = false;
    }
  }
  snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s}"), mqtt_data);
}

void MqttShowState()
{
  char stemp1[33];

  snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s{\"" D_JSON_TIME "\":\"%s\",\"" D_JSON_UPTIME "\":\"%s\""), mqtt_data, GetDateAndTime(DT_LOCAL).c_str(), GetUptime().c_str());
#ifdef USE_ADC_VCC
  dtostrfd((double)ESP.getVcc()/1000, 3, stemp1);
  snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s,\"" D_JSON_VCC "\":%s"), mqtt_data, stemp1);
#endif

  for (byte i = 0; i < devices_present; i++) {
    if (i == light_device -1) {
      LightState(1);
    } else {
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s,\"%s\":\"%s\""), mqtt_data, GetPowerDevice(stemp1, i +1, sizeof(stemp1), Settings.flag.device_index_enable), GetStateText(bitRead(power, i)));
      if (SONOFF_IFAN02 == Settings.module) {
        snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s,\"" D_CMND_FANSPEED "\":%d"), mqtt_data, GetFanspeed());
        break;
      }
    }
  }

  if (pwm_present) {
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s,"), mqtt_data);
    MqttShowPWMState();
  }

  snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s,\"" D_JSON_WIFI "\":{\"" D_JSON_AP "\":%d,\"" D_JSON_SSID "\":\"%s\",\"" D_JSON_BSSID "\":\"%s\",\"" D_JSON_CHANNEL "\":%d,\"" D_JSON_RSSI "\":%d}}"),
    mqtt_data, Settings.sta_active +1, Settings.sta_ssid[Settings.sta_active], WiFi.BSSIDstr().c_str(), WiFi.channel(), WifiGetRssiAsQuality(WiFi.RSSI()));
}

boolean MqttShowSensor()
{
  snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s{\"" D_JSON_TIME "\":\"%s\""), mqtt_data, GetDateAndTime(DT_LOCAL).c_str());
  int json_data_start = strlen(mqtt_data);
  for (byte i = 0; i < MAX_SWITCHES; i++) {
#ifdef USE_TM1638
    if ((pin[GPIO_SWT1 +i] < 99) || ((pin[GPIO_TM16CLK] < 99) && (pin[GPIO_TM16DIO] < 99) && (pin[GPIO_TM16STB] < 99))) {
#else
    if (pin[GPIO_SWT1 +i] < 99) {
#endif  // USE_TM1638
      boolean swm = ((FOLLOW_INV == Settings.switchmode[i]) || (PUSHBUTTON_INV == Settings.switchmode[i]) || (PUSHBUTTONHOLD_INV == Settings.switchmode[i]));
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s,\"" D_JSON_SWITCH "%d\":\"%s\""), mqtt_data, i +1, GetStateText(swm ^ lastwallswitch[i]));
    }
  }
  XsnsCall(FUNC_JSON_APPEND);
  boolean json_data_available = (strlen(mqtt_data) - json_data_start);
  if (strstr_P(mqtt_data, PSTR(D_JSON_PRESSURE))) {
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s,\"" D_JSON_PRESSURE_UNIT "\":\"%s\""), mqtt_data, PressureUnit().c_str());
  }
  if (strstr_P(mqtt_data, PSTR(D_JSON_TEMPERATURE))) {
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s,\"" D_JSON_TEMPERATURE_UNIT "\":\"%c\""), mqtt_data, TempUnit());
  }
  snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s}"), mqtt_data);

  if (json_data_available) XdrvCall(FUNC_SHOW_SENSOR);
  return json_data_available;
}

/********************************************************************************************/

void PerformEverySecond()
{
  uptime++;

  if (BOOT_LOOP_TIME == uptime) {
    RtcReboot.fast_reboot_count = 0;
    RtcRebootSave();
  }

  if ((4 == uptime) && (SONOFF_IFAN02 == Settings.module)) {  // Microcontroller needs 3 seconds before accepting commands
    SetDevicePower(1, SRC_RETRY);      // Sync with default power on state microcontroller being Light ON and Fan OFF
    SetDevicePower(power, SRC_RETRY);  // Set required power on state
  }

  if (seriallog_timer) {
    seriallog_timer--;
    if (!seriallog_timer) {
      if (seriallog_level) {
        AddLog_P(LOG_LEVEL_INFO, PSTR(D_LOG_APPLICATION D_SERIAL_LOGGING_DISABLED));
      }
      seriallog_level = 0;
    }
  }

  if (syslog_timer) {  // Restore syslog level
    syslog_timer--;
    if (!syslog_timer) {
      syslog_level = Settings.syslog_level;
      if (Settings.syslog_level) {
        AddLog_P(LOG_LEVEL_INFO, PSTR(D_LOG_APPLICATION D_SYSLOG_LOGGING_REENABLED));  // Might trigger disable again (on purpose)
      }
    }
  }

  ResetGlobalValues();

  if (Settings.tele_period) {
    tele_period++;
    if (tele_period == Settings.tele_period -1) {
      XsnsCall(FUNC_PREP_BEFORE_TELEPERIOD);
    }
    if (tele_period >= Settings.tele_period) {
      tele_period = 0;

      mqtt_data[0] = '\0';
      MqttShowState();
      MqttPublishPrefixTopic_P(TELE, PSTR(D_RSLT_STATE), MQTT_TELE_RETAIN);

      mqtt_data[0] = '\0';
      if (MqttShowSensor()) {
        MqttPublishPrefixTopic_P(TELE, PSTR(D_RSLT_SENSOR), Settings.flag.mqtt_sensor_retain);
#ifdef USE_RULES
        RulesTeleperiod();  // Allow rule based HA messages
#endif  // USE_RULES
      }
    }
  }

  XdrvCall(FUNC_EVERY_SECOND);
  XsnsCall(FUNC_EVERY_SECOND);

  if ((2 == RtcTime.minute) && latest_uptime_flag) {
    latest_uptime_flag = false;
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_JSON_TIME "\":\"%s\",\"" D_JSON_UPTIME "\":\"%s\"}"), GetDateAndTime(DT_LOCAL).c_str(), GetUptime().c_str());
    MqttPublishPrefixTopic_P(TELE, PSTR(D_RSLT_UPTIME));
  }
  if ((3 == RtcTime.minute) && !latest_uptime_flag) latest_uptime_flag = true;
}

/*********************************************************************************************\
 * Button handler with single press only or multi-press and hold on all buttons
\*********************************************************************************************/

void ButtonHandler()
{
  uint8_t button = NOT_PRESSED;
  uint8_t button_present = 0;
  uint8_t hold_time_extent = IMMINENT_RESET_FACTOR;            // Extent hold time factor in case of iminnent Reset command
  uint16_t loops_per_second = 1000 / Settings.button_debounce;
  char scmnd[20];

  uint8_t maxdev = (devices_present > MAX_KEYS) ? MAX_KEYS : devices_present;
  for (byte button_index = 0; button_index < maxdev; button_index++) {
    button = NOT_PRESSED;
    button_present = 0;

    if (!button_index && ((SONOFF_DUAL == Settings.module) || (CH4 == Settings.module))) {
      button_present = 1;
      if (dual_button_code) {
        snprintf_P(log_data, sizeof(log_data), PSTR(D_LOG_APPLICATION D_BUTTON " " D_CODE " %04X"), dual_button_code);
        AddLog(LOG_LEVEL_DEBUG);
        button = PRESSED;
        if (0xF500 == dual_button_code) {                      // Button hold
          holdbutton[button_index] = (loops_per_second * Settings.param[P_HOLD_TIME] / 10) -1;
          hold_time_extent = 1;
        }
        dual_button_code = 0;
      }
    } else {
      if (pin[GPIO_KEY1 +button_index] < 99) {
        if (!((uptime < 4) && (0 == pin[GPIO_KEY1 +button_index]))) {  // Block GPIO0 for 4 seconds after poweron to workaround Wemos D1 RTS circuit
          button_present = 1;
          button = digitalRead(pin[GPIO_KEY1 +button_index]);
        }
      }
    }

    if (button_present) {
      XdrvMailbox.index = button_index;
      XdrvMailbox.payload = button;
      if (XdrvCall(FUNC_BUTTON_PRESSED)) {
        // Serviced
      }
      else if (SONOFF_4CHPRO == Settings.module) {
        if (holdbutton[button_index]) { holdbutton[button_index]--; }

        boolean button_pressed = false;
        if ((PRESSED == button) && (NOT_PRESSED == lastbutton[button_index])) {
          snprintf_P(log_data, sizeof(log_data), PSTR(D_LOG_APPLICATION D_BUTTON "%d " D_LEVEL_10), button_index +1);
          AddLog(LOG_LEVEL_DEBUG);
          holdbutton[button_index] = loops_per_second;
          button_pressed = true;
        }
        if ((NOT_PRESSED == button) && (PRESSED == lastbutton[button_index])) {
          snprintf_P(log_data, sizeof(log_data), PSTR(D_LOG_APPLICATION D_BUTTON "%d " D_LEVEL_01), button_index +1);
          AddLog(LOG_LEVEL_DEBUG);
          if (!holdbutton[button_index]) { button_pressed = true; }  // Do not allow within 1 second
        }
        if (button_pressed) {
          if (!SendKey(0, button_index +1, POWER_TOGGLE)) {    // Execute Toggle command via MQTT if ButtonTopic is set
            ExecuteCommandPower(button_index +1, POWER_TOGGLE, SRC_BUTTON);  // Execute Toggle command internally
          }
        }
      }
      else {
        if ((PRESSED == button) && (NOT_PRESSED == lastbutton[button_index])) {
          if (Settings.flag.button_single) {                   // Allow only single button press for immediate action
            snprintf_P(log_data, sizeof(log_data), PSTR(D_LOG_APPLICATION D_BUTTON "%d " D_IMMEDIATE), button_index +1);
            AddLog(LOG_LEVEL_DEBUG);
            if (!SendKey(0, button_index +1, POWER_TOGGLE)) {  // Execute Toggle command via MQTT if ButtonTopic is set
              ExecuteCommandPower(button_index +1, POWER_TOGGLE, SRC_BUTTON);  // Execute Toggle command internally
            }
          } else {
            multipress[button_index] = (multiwindow[button_index]) ? multipress[button_index] +1 : 1;
            snprintf_P(log_data, sizeof(log_data), PSTR(D_LOG_APPLICATION D_BUTTON "%d " D_MULTI_PRESS " %d"), button_index +1, multipress[button_index]);
            AddLog(LOG_LEVEL_DEBUG);
            multiwindow[button_index] = loops_per_second / 2;  // 0.5 second multi press window
          }
          blinks = 201;
        }

        if (NOT_PRESSED == button) {
          holdbutton[button_index] = 0;
        } else {
          holdbutton[button_index]++;
          if (Settings.flag.button_single) {                   // Allow only single button press for immediate action
            if (holdbutton[button_index] == loops_per_second * hold_time_extent * Settings.param[P_HOLD_TIME] / 10) {  // Button held for factor times longer
//              Settings.flag.button_single = 0;
              snprintf_P(scmnd, sizeof(scmnd), PSTR(D_CMND_SETOPTION "13 0"));  // Disable single press only
              ExecuteCommand(scmnd, SRC_BUTTON);
            }
          } else {
            if (Settings.flag.button_restrict) {               // Button restriction
              if (holdbutton[button_index] == loops_per_second * Settings.param[P_HOLD_TIME] / 10) {  // Button hold
                multipress[button_index] = 0;
                SendKey(0, button_index +1, 3);                // Execute Hold command via MQTT if ButtonTopic is set
              }
            } else {
              if (holdbutton[button_index] == loops_per_second * hold_time_extent * Settings.param[P_HOLD_TIME] / 10) {  // Button held for factor times longer
                multipress[button_index] = 0;
                snprintf_P(scmnd, sizeof(scmnd), PSTR(D_CMND_RESET " 1"));
                ExecuteCommand(scmnd, SRC_BUTTON);
              }
            }
          }
        }

        if (!Settings.flag.button_single) {                    // Allow multi-press
          if (multiwindow[button_index]) {
            multiwindow[button_index]--;
          } else {
            if (!restart_flag && !holdbutton[button_index] && (multipress[button_index] > 0) && (multipress[button_index] < MAX_BUTTON_COMMANDS +3)) {
              boolean single_press = false;
              if (multipress[button_index] < 3) {              // Single or Double press
                if ((SONOFF_DUAL_R2 == Settings.module) || (SONOFF_DUAL == Settings.module) || (CH4 == Settings.module)) {
                  single_press = true;
                } else  {
                  single_press = (Settings.flag.button_swap +1 == multipress[button_index]);
                  multipress[button_index] = 1;
                }
              }
              if (single_press && SendKey(0, button_index + multipress[button_index], POWER_TOGGLE)) {  // Execute Toggle command via MQTT if ButtonTopic is set
                // Success
              } else {
                if (multipress[button_index] < 3) {            // Single or Double press
                  if (WifiState() > WIFI_RESTART) {            // WPSconfig, Smartconfig or Wifimanager active
                    restart_flag = 1;
                  } else {
                    ExecuteCommandPower(button_index + multipress[button_index], POWER_TOGGLE, SRC_BUTTON);  // Execute Toggle command internally
                  }
                } else {                                       // 3 - 7 press
                  if (!Settings.flag.button_restrict) {
                    snprintf_P(scmnd, sizeof(scmnd), kCommands[multipress[button_index] -3]);
                    ExecuteCommand(scmnd, SRC_BUTTON);
                  }
                }
              }
              multipress[button_index] = 0;
            }
          }
        }
      }
    }
    lastbutton[button_index] = button;
  }
}

/*********************************************************************************************\
 * Switch handler
\*********************************************************************************************/

void SwitchHandler(byte mode)
{
  uint8_t button = NOT_PRESSED;
  uint8_t switchflag;
  uint16_t loops_per_second = 1000 / Settings.switch_debounce;

  for (byte i = 0; i < MAX_SWITCHES; i++) {
    if ((pin[GPIO_SWT1 +i] < 99) || (mode)) {

      if (holdwallswitch[i]) {
        holdwallswitch[i]--;
        if (0 == holdwallswitch[i]) {
          SendKey(1, i +1, 3);           // Execute command via MQTT
        }
      }

      if (mode) {
        button = virtualswitch[i];
      } else {
        if (!((uptime < 4) && (0 == pin[GPIO_SWT1 +i]))) {  // Block GPIO0 for 4 seconds after poweron to workaround Wemos D1 RTS circuit
          button = digitalRead(pin[GPIO_SWT1 +i]);
        }
      }

      if (button != lastwallswitch[i]) {
        switchflag = 3;
        switch (Settings.switchmode[i]) {
        case TOGGLE:
          switchflag = 2;                // Toggle
          break;
        case FOLLOW:
          switchflag = button &1;        // Follow wall switch state
          break;
        case FOLLOW_INV:
          switchflag = ~button &1;       // Follow inverted wall switch state
          break;
        case PUSHBUTTON:
          if ((PRESSED == button) && (NOT_PRESSED == lastwallswitch[i])) {
            switchflag = 2;              // Toggle with pushbutton to Gnd
          }
          break;
        case PUSHBUTTON_INV:
          if ((NOT_PRESSED == button) && (PRESSED == lastwallswitch[i])) {
            switchflag = 2;              // Toggle with releasing pushbutton from Gnd
          }
          break;
        case PUSHBUTTON_TOGGLE:
          if (button != lastwallswitch[i]) {
            switchflag = 2;              // Toggle with any pushbutton change
          }
          break;
        case PUSHBUTTONHOLD:
          if ((PRESSED == button) && (NOT_PRESSED == lastwallswitch[i])) {
            holdwallswitch[i] = loops_per_second * Settings.param[P_HOLD_TIME] / 10;
          }
          if ((NOT_PRESSED == button) && (PRESSED == lastwallswitch[i]) && (holdwallswitch[i])) {
            holdwallswitch[i] = 0;
            switchflag = 2;              // Toggle with pushbutton to Gnd
          }
          break;
        case PUSHBUTTONHOLD_INV:
          if ((NOT_PRESSED == button) && (PRESSED == lastwallswitch[i])) {
            holdwallswitch[i] = loops_per_second * Settings.param[P_HOLD_TIME] / 10;
          }
          if ((PRESSED == button) && (NOT_PRESSED == lastwallswitch[i]) && (holdwallswitch[i])) {
            holdwallswitch[i] = 0;
            switchflag = 2;              // Toggle with pushbutton to Gnd
          }
          break;
        }

        if (switchflag < 3) {
          if (!SendKey(1, i +1, switchflag)) {  // Execute command via MQTT
            ExecuteCommandPower(i +1, switchflag, SRC_SWITCH);  // Execute command internally (if i < devices_present)
          }
        }

        lastwallswitch[i] = button;
      }
    }
  }
}

/*********************************************************************************************\
 * State loops
\*********************************************************************************************/
/*-------------------------------------------------------------------------------------------*\
 * Every 0.1 second
\*-------------------------------------------------------------------------------------------*/

void Every100mSeconds()
{
  // As the max amount of sleep = 250 mSec this loop will shift in time...
  power_t power_now;

  if (latching_relay_pulse) {
    latching_relay_pulse--;
    if (!latching_relay_pulse) SetLatchingRelay(0, 0);
  }

  for (byte i = 0; i < MAX_PULSETIMERS; i++) {
    if (pulse_timer[i] != 0L) {           // Timer active?
      if (TimeReached(pulse_timer[i])) {  // Timer finished?
        pulse_timer[i] = 0L;              // Turn off this timer
        ExecuteCommandPower(i +1, (POWER_ALL_OFF_PULSETIME_ON == Settings.poweronstate) ? POWER_ON : POWER_OFF, SRC_PULSETIMER);
      }
    }
  }

  if (blink_mask) {
    if (TimeReached(blink_timer)) {
      SetNextTimeInterval(blink_timer, 100 * Settings.blinktime);
      blink_counter--;
      if (!blink_counter) {
        StopAllPowerBlink();
      } else {
        blink_power ^= 1;
        power_now = (power & (POWER_MASK ^ blink_mask)) | ((blink_power) ? blink_mask : 0);
        SetDevicePower(power_now, SRC_IGNORE);
      }
    }
  }

  // Backlog
  if (TimeReached(backlog_delay)) {
    if ((backlog_pointer != backlog_index) && !backlog_mutex) {
      backlog_mutex = 1;
      ExecuteCommand((char*)backlog[backlog_pointer].c_str(), SRC_BACKLOG);
      backlog_mutex = 0;
      backlog_pointer++;
      if (backlog_pointer >= MAX_BACKLOG) { backlog_pointer = 0; }
    }
  }
}

/*-------------------------------------------------------------------------------------------*\
 * Every 0.25 second
\*-------------------------------------------------------------------------------------------*/

void Every250mSeconds()
{
// As the max amount of sleep = 250 mSec this loop should always be taken...

  uint8_t blinkinterval = 1;

  state_250mS++;
  state_250mS &= 0x3;

  if (mqtt_cmnd_publish) mqtt_cmnd_publish--;             // Clean up

  if (!Settings.flag.global_state) {                      // Problem blinkyblinky enabled
    if (global_state.data) {                              // Any problem
      if (global_state.mqtt_down) { blinkinterval = 7; }  // MQTT problem so blink every 2 seconds (slowest)
      if (global_state.wifi_down) { blinkinterval = 3; }  // Wifi problem so blink every second (slow)
      blinks = 201;                                       // Allow only a single blink in case the problem is solved
    }
  }
  if (blinks || restart_flag || ota_state_flag) {
    if (restart_flag || ota_state_flag) {                 // Overrule blinks and keep led lit
      blinkstate = 1;                                     // Stay lit
    } else {
      blinkspeed--;
      if (!blinkspeed) {
        blinkspeed = blinkinterval;                       // Set interval to 0.2 (default), 1 or 2 seconds
        blinkstate ^= 1;                                  // Blink
      }
    }
    if ((!(Settings.ledstate &0x08)) && ((Settings.ledstate &0x06) || (blinks > 200) || (blinkstate))) {
//    if ( (!Settings.flag.global_state && global_state.data) || ((!(Settings.ledstate &0x08)) && ((Settings.ledstate &0x06) || (blinks > 200) || (blinkstate))) ) {
      SetLedPower(blinkstate);                            // Set led on or off
    }
    if (!blinkstate) {
      blinks--;
      if (200 == blinks) blinks = 0;                      // Disable blink
    }
  }
  else if (Settings.ledstate &1) {
    boolean tstate = power;
    if ((SONOFF_TOUCH == Settings.module) || (SONOFF_T11 == Settings.module) || (SONOFF_T12 == Settings.module) || (SONOFF_T13 == Settings.module)) {
      tstate = (!power) ? 1 : 0;                          // As requested invert signal for Touch devices to find them in the dark
    }
    SetLedPower(tstate);
  }

/*-------------------------------------------------------------------------------------------*\
 * Every second at 0.25 second interval
\*-------------------------------------------------------------------------------------------*/

  switch (state_250mS) {
  case 0:                                                 // Every x.0 second
    PerformEverySecond();

    if (ota_state_flag && (backlog_pointer == backlog_index)) {
      ota_state_flag--;
      if (2 == ota_state_flag) {
        ota_url = Settings.ota_url;
        RtcSettings.ota_loader = 0;  // Try requested image first
        ota_retry_counter = OTA_ATTEMPTS;
        ESPhttpUpdate.rebootOnUpdate(false);
        SettingsSave(1);  // Free flash for OTA update
      }
      if (ota_state_flag <= 0) {
#ifdef USE_WEBSERVER
        if (Settings.webserver) StopWebserver();
#endif  // USE_WEBSERVER
#ifdef USE_ARILUX_RF
        AriluxRfDisable();  // Prevent restart exception on Arilux Interrupt routine
#endif  // USE_ARILUX_RF
        ota_state_flag = 92;
        ota_result = 0;
        ota_retry_counter--;
        if (ota_retry_counter) {
          strlcpy(mqtt_data, GetOtaUrl(log_data, sizeof(log_data)), sizeof(mqtt_data));
#ifndef BE_MINIMAL
          if (RtcSettings.ota_loader) {
            char *bch = strrchr(mqtt_data, '/');                        // Only consider filename after last backslash prevent change of urls having "-" in it
            char *pch = strrchr((bch != NULL) ? bch : mqtt_data, '-');  // Change from filename-DE.bin into filename-minimal.bin
            char *ech = strrchr((bch != NULL) ? bch : mqtt_data, '.');  // Change from filename.bin into filename-minimal.bin
            if (!pch) pch = ech;
            if (pch) {
              mqtt_data[pch - mqtt_data] = '\0';
              char *ech = strrchr(Settings.ota_url, '.');  // Change from filename.bin into filename-minimal.bin
              snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s-" D_JSON_MINIMAL "%s"), mqtt_data, ech);  // Minimal filename must be filename-minimal
            }
          }
#endif  // BE_MINIMAL
          snprintf_P(log_data, sizeof(log_data), PSTR(D_LOG_UPLOAD "%s"), mqtt_data);
          AddLog(LOG_LEVEL_DEBUG);
#if defined(ARDUINO_ESP8266_RELEASE_2_3_0) || defined(ARDUINO_ESP8266_RELEASE_2_4_0) || defined(ARDUINO_ESP8266_RELEASE_2_4_1) || defined(ARDUINO_ESP8266_RELEASE_2_4_2)
          ota_result = (HTTP_UPDATE_FAILED != ESPhttpUpdate.update(mqtt_data));
#else
          // If using core stage or 2.5.0+ the syntax has changed
          WiFiClient OTAclient;
          ota_result = (HTTP_UPDATE_FAILED != ESPhttpUpdate.update(OTAclient, mqtt_data));
#endif
          if (!ota_result) {
#ifndef BE_MINIMAL
            int ota_error = ESPhttpUpdate.getLastError();
//            snprintf_P(log_data, sizeof(log_data), PSTR(D_LOG_UPLOAD "Ota error %d"), ota_error);
//            AddLog(LOG_LEVEL_DEBUG);
            if ((HTTP_UE_TOO_LESS_SPACE == ota_error) || (HTTP_UE_BIN_FOR_WRONG_FLASH == ota_error)) {
              RtcSettings.ota_loader = 1;  // Try minimal image next
            }
#endif  // BE_MINIMAL
            ota_state_flag = 2;    // Upgrade failed - retry
          }
        }
      }
      if (90 == ota_state_flag) {  // Allow MQTT to reconnect
        ota_state_flag = 0;
        if (ota_result) {
          SetFlashModeDout();      // Force DOUT for both ESP8266 and ESP8285
          snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR(D_JSON_SUCCESSFUL ". " D_JSON_RESTARTING));
        } else {
          snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR(D_JSON_FAILED " %s"), ESPhttpUpdate.getLastErrorString().c_str());
        }
        restart_flag = 2;          // Restart anyway to keep memory clean webserver
        MqttPublishPrefixTopic_P(STAT, PSTR(D_CMND_UPGRADE));
      }
    }
    break;
  case 1:                                                 // Every x.25 second
    if (MidnightNow()) { CounterSaveState(); }
    if (save_data_counter && (backlog_pointer == backlog_index)) {
      save_data_counter--;
      if (save_data_counter <= 0) {
        if (Settings.flag.save_state) {
          power_t mask = POWER_MASK;
          for (byte i = 0; i < MAX_PULSETIMERS; i++) {
            if ((Settings.pulse_timer[i] > 0) && (Settings.pulse_timer[i] < 30)) {  // 3 seconds
              mask &= ~(1 << i);
            }
          }
          if (!((Settings.power &mask) == (power &mask))) {
            Settings.power = power;
          }
        } else {
          Settings.power = 0;
        }
        SettingsSave(0);
        save_data_counter = Settings.save_data;
      }
    }
    if (restart_flag && (backlog_pointer == backlog_index)) {
      if ((214 == restart_flag) || (215 == restart_flag)) {
        char storage[sizeof(Settings.sta_ssid) + sizeof(Settings.sta_pwd)];
        memcpy(storage, Settings.sta_ssid, sizeof(storage));  // Backup current SSIDs and Passwords
        if (215 == restart_flag) {
          SettingsErase(0);  // Erase all flash from program end to end of physical flash
        }
        SettingsDefault();
        memcpy(Settings.sta_ssid, storage, sizeof(storage));  // Restore current SSIDs and Passwords
        restart_flag = 2;
      }
      else if (213 == restart_flag) {
        SettingsSdkErase();  // Erase flash SDK parameters
        restart_flag = 2;
      }
      else if (212 == restart_flag) {
        SettingsErase(0);    // Erase all flash from program end to end of physical flash
        restart_flag = 211;
      }
      if (211 == restart_flag) {
        SettingsDefault();
        restart_flag = 2;
      }
      SettingsSaveAll();
      restart_flag--;
      if (restart_flag <= 0) {
        AddLog_P(LOG_LEVEL_INFO, PSTR(D_LOG_APPLICATION D_RESTARTING));
        EspRestart();
      }
    }
    break;
  case 2:                                                 // Every x.5 second
    WifiCheck(wifi_state_flag);
    wifi_state_flag = WIFI_RESTART;
    break;
  case 3:                                                 // Every x.75 second
    if (!global_state.wifi_down) { MqttCheck(); }
    break;
  }
}

#ifdef USE_ARDUINO_OTA
/*********************************************************************************************\
 * Allow updating via the Arduino OTA-protocol.
 *
 * - Once started disables current wifi clients and udp
 * - Perform restart when done to re-init wifi clients
\*********************************************************************************************/

bool arduino_ota_triggered = false;
uint16_t arduino_ota_progress_dot_count = 0;

void ArduinoOTAInit()
{
  ArduinoOTA.setPort(8266);
  ArduinoOTA.setHostname(Settings.hostname);
  if (Settings.web_password[0] !=0) ArduinoOTA.setPassword(Settings.web_password);

  ArduinoOTA.onStart([]()
  {
    SettingsSave(1);  // Free flash for OTA update
#ifdef USE_WEBSERVER
    if (Settings.webserver) StopWebserver();
#endif  // USE_WEBSERVER
#ifdef USE_ARILUX_RF
    AriluxRfDisable();  // Prevent restart exception on Arilux Interrupt routine
#endif  // USE_ARILUX_RF
    if (Settings.flag.mqtt_enabled) MqttDisconnect();
    snprintf_P(log_data, sizeof(log_data), PSTR(D_LOG_UPLOAD "Arduino OTA " D_UPLOAD_STARTED));
    AddLog(LOG_LEVEL_INFO);
    arduino_ota_triggered = true;
    arduino_ota_progress_dot_count = 0;
    delay(100);       // Allow time for message xfer
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
  {
    if ((LOG_LEVEL_DEBUG <= seriallog_level)) {
      arduino_ota_progress_dot_count++;
      Serial.printf(".");
      if (!(arduino_ota_progress_dot_count % 80)) Serial.println();
    }
  });

  ArduinoOTA.onError([](ota_error_t error)
  {
    /*
    From ArduinoOTA.h:
    typedef enum { OTA_AUTH_ERROR, OTA_BEGIN_ERROR, OTA_CONNECT_ERROR, OTA_RECEIVE_ERROR, OTA_END_ERROR } ota_error_t;
    */
    char error_str[100];

    if ((LOG_LEVEL_DEBUG <= seriallog_level) && arduino_ota_progress_dot_count) Serial.println();
    switch (error) {
      case OTA_BEGIN_ERROR: strncpy_P(error_str, PSTR(D_UPLOAD_ERR_2), sizeof(error_str)); break;
      case OTA_RECEIVE_ERROR: strncpy_P(error_str, PSTR(D_UPLOAD_ERR_5), sizeof(error_str)); break;
      case OTA_END_ERROR: strncpy_P(error_str, PSTR(D_UPLOAD_ERR_7), sizeof(error_str)); break;
      default:
        snprintf_P(error_str, sizeof(error_str), PSTR(D_UPLOAD_ERROR_CODE " %d"), error);
    }
    snprintf_P(log_data, sizeof(log_data), PSTR(D_LOG_UPLOAD "Arduino OTA  %s. " D_RESTARTING), error_str);
    AddLog(LOG_LEVEL_INFO);
    EspRestart();
  });

  ArduinoOTA.onEnd([]()
  {
    if ((LOG_LEVEL_DEBUG <= seriallog_level)) Serial.println();
    snprintf_P(log_data, sizeof(log_data), PSTR(D_LOG_UPLOAD "Arduino OTA " D_SUCCESSFUL ". " D_RESTARTING));
    AddLog(LOG_LEVEL_INFO);
    EspRestart();
	});

  ArduinoOTA.begin();
  snprintf_P(log_data, sizeof(log_data), PSTR(D_LOG_UPLOAD "Arduino OTA " D_ENABLED " " D_PORT " 8266"));
  AddLog(LOG_LEVEL_INFO);
}
#endif  // USE_ARDUINO_OTA

/********************************************************************************************/

void SerialInput()
{
  while (Serial.available()) {
    yield();
    serial_in_byte = Serial.read();

/*-------------------------------------------------------------------------------------------*\
 * Sonoff dual and ch4 19200 baud serial interface
\*-------------------------------------------------------------------------------------------*/
    if ((SONOFF_DUAL == Settings.module) || (CH4 == Settings.module)) {
      if (dual_hex_code) {
        dual_hex_code--;
        if (dual_hex_code) {
          dual_button_code = (dual_button_code << 8) | serial_in_byte;
          serial_in_byte = 0;
        } else {
          if (serial_in_byte != 0xA1) {
            dual_button_code = 0;                // 0xA1 - End of Sonoff dual button code
          }
        }
      }
      if (0xA0 == serial_in_byte) {              // 0xA0 - Start of Sonoff dual button code
        serial_in_byte = 0;
        dual_button_code = 0;
        dual_hex_code = 3;
      }
    }

/*-------------------------------------------------------------------------------------------*/

    if (XdrvCall(FUNC_SERIAL)) {
      serial_in_byte_counter = 0;
      Serial.flush();
      return;
    }

/*-------------------------------------------------------------------------------------------*/

    if (serial_in_byte > 127 && !Settings.flag.mqtt_serial_raw) { // binary data...
      serial_in_byte_counter = 0;
      Serial.flush();
      return;
    }
    if (!Settings.flag.mqtt_serial) {
      if (isprint(serial_in_byte)) {
        if (serial_in_byte_counter < INPUT_BUFFER_SIZE -1) {  // add char to string if it still fits
          serial_in_buffer[serial_in_byte_counter++] = serial_in_byte;
        } else {
          serial_in_byte_counter = 0;
        }
      }
    } else {
      if (serial_in_byte || Settings.flag.mqtt_serial_raw) {
        if ((serial_in_byte_counter < INPUT_BUFFER_SIZE -1) &&
            ((serial_in_byte != Settings.serial_delimiter) || Settings.flag.mqtt_serial_raw)) {  // add char to string if it still fits
          serial_in_buffer[serial_in_byte_counter++] = serial_in_byte;
          serial_polling_window = millis();
        } else {
          serial_polling_window = 0;
          break;
        }
      }
    }

/*-------------------------------------------------------------------------------------------*\
 * Sonoff SC 19200 baud serial interface
\*-------------------------------------------------------------------------------------------*/
    if (SONOFF_SC == Settings.module) {
      if (serial_in_byte == '\x1B') {            // Sonoff SC status from ATMEGA328P
        serial_in_buffer[serial_in_byte_counter] = 0;  // serial data completed
        SonoffScSerialInput(serial_in_buffer);
        serial_in_byte_counter = 0;
        Serial.flush();
        return;
      }
    }

/*-------------------------------------------------------------------------------------------*/

    else if (!Settings.flag.mqtt_serial && (serial_in_byte == '\n')) {
      serial_in_buffer[serial_in_byte_counter] = 0;  // serial data completed
      seriallog_level = (Settings.seriallog_level < LOG_LEVEL_INFO) ? (byte)LOG_LEVEL_INFO : Settings.seriallog_level;
      snprintf_P(log_data, sizeof(log_data), PSTR(D_LOG_COMMAND "%s"), serial_in_buffer);
      AddLog(LOG_LEVEL_INFO);
      ExecuteCommand(serial_in_buffer, SRC_SERIAL);
      serial_in_byte_counter = 0;
      serial_polling_window = 0;
      Serial.flush();
      return;
    }
  }

  if (Settings.flag.mqtt_serial && serial_in_byte_counter && (millis() > (serial_polling_window + SERIAL_POLLING))) {
    serial_in_buffer[serial_in_byte_counter] = 0;  // serial data completed
    if (!Settings.flag.mqtt_serial_raw) {
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_JSON_SERIALRECEIVED "\":\"%s\"}"), serial_in_buffer);
    } else {
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_JSON_SERIALRECEIVED "\":\""));
      for (int i = 0; i < serial_in_byte_counter; i++) {
        snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s%02x"), mqtt_data, serial_in_buffer[i]);
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s\"}"), mqtt_data);
    }
    MqttPublishPrefixTopic_P(RESULT_OR_TELE, PSTR(D_JSON_SERIALRECEIVED));
//      XdrvRulesProcess();
    serial_in_byte_counter = 0;
  }
}
/********************************************************************************************/

void GpioSwitchPinMode(uint8_t index)
{
  if (pin[GPIO_SWT1 +index] < 99) {
    pinMode(pin[GPIO_SWT1 +index], (16 == pin[GPIO_SWT1 +index]) ? INPUT_PULLDOWN_16 : bitRead(switch_no_pullup, index) ? INPUT : INPUT_PULLUP);
/*
    // Re-enable pull-up on Shelly2 as of 20181110 (#4255)
    uint8_t no_pullup = bitRead(switch_no_pullup, index);       // 0 = INPUT_PULLUP, 1 = INPUT
    if (no_pullup) {
      if (SHELLY2 == Settings.module) {
        // Switchmodes : TOGGLE, FOLLOW, FOLLOW_INV, PUSHBUTTON, PUSHBUTTON_INV, PUSHBUTTONHOLD, PUSHBUTTONHOLD_INV, PUSHBUTTON_TOGGLE, MAX_SWITCH_OPTION
        no_pullup = (Settings.switchmode[index] < PUSHBUTTON);  // INPUT on TOGGLE, FOLLOW and FOLLOW_INV. INPUT_PULLUP on anything else
      }
    }
    pinMode(pin[GPIO_SWT1 +index], (16 == pin[GPIO_SWT1 +index]) ? INPUT_PULLDOWN_16 : (no_pullup) ? INPUT : INPUT_PULLUP);
*/
  }
}

void GpioInit()
{
  uint8_t mpin;
  uint8_t key_no_pullup = 0;
  mytmplt def_module;

  if (Settings.module >= MAXMODULE) {
    Settings.module = MODULE;
    Settings.last_module = MODULE;
  }
  if (Settings.module != Settings.last_module) {
    baudrate = APP_BAUDRATE;
  }

  memcpy_P(&def_module, &kModules[Settings.module], sizeof(def_module));
  strlcpy(my_module.name, def_module.name, sizeof(my_module.name));
  for (byte i = 0; i < MAX_GPIO_PIN; i++) {
    if (Settings.my_gp.io[i] > GPIO_NONE) {
      my_module.gp.io[i] = Settings.my_gp.io[i];
    }
    if ((def_module.gp.io[i] > GPIO_NONE) && (def_module.gp.io[i] < GPIO_USER)) {
      my_module.gp.io[i] = def_module.gp.io[i];
    }
  }

  for (byte i = 0; i < GPIO_MAX; i++) {
    pin[i] = 99;
  }
  for (byte i = 0; i < MAX_GPIO_PIN; i++) {
    mpin = ValidGPIO(i, my_module.gp.io[i]);

//  snprintf_P(log_data, sizeof(log_data), PSTR("DBG: gpio pin %d, mpin %d"), i, mpin);
//  AddLog(LOG_LEVEL_DEBUG);

    if (mpin) {
      if ((mpin >= GPIO_SWT1_NP) && (mpin < (GPIO_SWT1_NP + MAX_SWITCHES))) {
        bitSet(switch_no_pullup, mpin - GPIO_SWT1_NP);
        mpin -= (GPIO_SWT1_NP - GPIO_SWT1);
      }
      else if ((mpin >= GPIO_KEY1_NP) && (mpin < (GPIO_KEY1_NP + MAX_KEYS))) {
        bitSet(key_no_pullup, mpin - GPIO_KEY1_NP);
        mpin -= (GPIO_KEY1_NP - GPIO_KEY1);
      }
      else if ((mpin >= GPIO_REL1_INV) && (mpin < (GPIO_REL1_INV + MAX_RELAYS))) {
        bitSet(rel_inverted, mpin - GPIO_REL1_INV);
        mpin -= (GPIO_REL1_INV - GPIO_REL1);
      }
      else if ((mpin >= GPIO_LED1_INV) && (mpin < (GPIO_LED1_INV + MAX_LEDS))) {
        bitSet(led_inverted, mpin - GPIO_LED1_INV);
        mpin -= (GPIO_LED1_INV - GPIO_LED1);
      }
      else if ((mpin >= GPIO_PWM1_INV) && (mpin < (GPIO_PWM1_INV + MAX_PWMS))) {
        bitSet(pwm_inverted, mpin - GPIO_PWM1_INV);
        mpin -= (GPIO_PWM1_INV - GPIO_PWM1);
      }
      else if ((mpin >= GPIO_CNTR1_NP) && (mpin < (GPIO_CNTR1_NP + MAX_COUNTERS))) {
        bitSet(counter_no_pullup, mpin - GPIO_CNTR1_NP);
        mpin -= (GPIO_CNTR1_NP - GPIO_CNTR1);
      }
#ifdef USE_DHT
      else if ((mpin >= GPIO_DHT11) && (mpin <= GPIO_SI7021)) {
        if (DhtSetup(i, mpin)) {
          dht_flg = 1;
          mpin = GPIO_DHT11;
        } else {
          mpin = 0;
        }
      }
#endif  // USE_DHT
    }
    if (mpin) pin[mpin] = i;
  }

  if ((2 == pin[GPIO_TXD]) || (H801 == Settings.module)) { Serial.set_tx(2); }

  analogWriteRange(Settings.pwm_range);      // Default is 1023 (Arduino.h)
  analogWriteFreq(Settings.pwm_frequency);   // Default is 1000 (core_esp8266_wiring_pwm.c)

#ifdef USE_SPI
  spi_flg = ((((pin[GPIO_SPI_CS] < 99) && (pin[GPIO_SPI_CS] > 14)) || (pin[GPIO_SPI_CS] < 12)) || (((pin[GPIO_SPI_DC] < 99) && (pin[GPIO_SPI_DC] > 14)) || (pin[GPIO_SPI_DC] < 12)));
  if (spi_flg) {
    for (byte i = 0; i < GPIO_MAX; i++) {
      if ((pin[i] >= 12) && (pin[i] <=14)) pin[i] = 99;
    }
    my_module.gp.io[12] = GPIO_SPI_MISO;
    pin[GPIO_SPI_MISO] = 12;
    my_module.gp.io[13] = GPIO_SPI_MOSI;
    pin[GPIO_SPI_MOSI] = 13;
    my_module.gp.io[14] = GPIO_SPI_CLK;
    pin[GPIO_SPI_CLK] = 14;
  }
#endif  // USE_SPI

#ifdef USE_I2C
  i2c_flg = ((pin[GPIO_I2C_SCL] < 99) && (pin[GPIO_I2C_SDA] < 99));
  if (i2c_flg) Wire.begin(pin[GPIO_I2C_SDA], pin[GPIO_I2C_SCL]);
#endif  // USE_I2C

  devices_present = 1;

  light_type = LT_BASIC;                     // Use basic PWM control if SetOption15 = 0
  if (Settings.flag.pwm_control) {
    for (byte i = 0; i < MAX_PWMS; i++) {
      if (pin[GPIO_PWM1 +i] < 99) light_type++;  // Use Dimmer/Color control for all PWM as SetOption15 = 1
    }
  }

  if (SONOFF_BRIDGE == Settings.module) {
    Settings.flag.mqtt_serial = 0;
    baudrate = 19200;
  }

  if (XdrvCall(FUNC_MODULE_INIT)) {
    // Serviced
  }
  else if (SONOFF_DUAL == Settings.module) {
    Settings.flag.mqtt_serial = 0;
    devices_present = 2;
    baudrate = 19200;
  }
  else if (CH4 == Settings.module) {
    Settings.flag.mqtt_serial = 0;
    devices_present = 4;
    baudrate = 19200;
  }
  else if (SONOFF_SC == Settings.module) {
    Settings.flag.mqtt_serial = 0;
    devices_present = 0;
    baudrate = 19200;
  }
  else if (SONOFF_BN == Settings.module) {   // PWM Single color led (White)
    light_type = LT_PWM1;
  }
  else if (SONOFF_LED == Settings.module) {  // PWM Dual color led (White warm and cold)
    light_type = LT_PWM2;
  }
  else if (AILIGHT == Settings.module) {     // RGBW led
    light_type = LT_RGBW;
  }
  else if (SONOFF_B1 == Settings.module) {   // RGBWC led
    light_type = LT_RGBWC;
  }
  else {
    if (!light_type) devices_present = 0;
    for (byte i = 0; i < MAX_RELAYS; i++) {
      if (pin[GPIO_REL1 +i] < 99) {
        pinMode(pin[GPIO_REL1 +i], OUTPUT);
        devices_present++;
        if (EXS_RELAY == Settings.module) {
          digitalWrite(pin[GPIO_REL1 +i], bitRead(rel_inverted, i) ? 1 : 0);
          if (i &1) { devices_present--; }
        }
      }
    }
  }

  for (byte i = 0; i < MAX_KEYS; i++) {
    if (pin[GPIO_KEY1 +i] < 99) {
      pinMode(pin[GPIO_KEY1 +i], (16 == pin[GPIO_KEY1 +i]) ? INPUT_PULLDOWN_16 : bitRead(key_no_pullup, i) ? INPUT : INPUT_PULLUP);
    }
  }
  for (byte i = 0; i < MAX_LEDS; i++) {
    if (pin[GPIO_LED1 +i] < 99) {
      pinMode(pin[GPIO_LED1 +i], OUTPUT);
      digitalWrite(pin[GPIO_LED1 +i], bitRead(led_inverted, i));
    }
  }
  for (byte i = 0; i < MAX_SWITCHES; i++) {
    lastwallswitch[i] = 1;  // Init global to virtual switch state;
    if (pin[GPIO_SWT1 +i] < 99) {
      GpioSwitchPinMode(i);
      lastwallswitch[i] = digitalRead(pin[GPIO_SWT1 +i]);  // Set global now so doesn't change the saved power state on first switch check
    }
    virtualswitch[i] = lastwallswitch[i];
  }

#ifdef USE_WS2812
  if (!light_type && (pin[GPIO_WS2812] < 99)) {  // RGB led
    devices_present++;
    light_type = LT_WS2812;
  }
#endif  // USE_WS2812
  if (!light_type) {
    for (byte i = 0; i < MAX_PWMS; i++) {     // Basic PWM control only
      if (pin[GPIO_PWM1 +i] < 99) {
        pwm_present = true;
        pinMode(pin[GPIO_PWM1 +i], OUTPUT);
        analogWrite(pin[GPIO_PWM1 +i], bitRead(pwm_inverted, i) ? Settings.pwm_range - Settings.pwm_value[i] : Settings.pwm_value[i]);
      }
    }
  }

  SetLedPower(Settings.ledstate &8);

  XdrvCall(FUNC_PRE_INIT);
}

extern "C" {
extern struct rst_info resetInfo;
}

void setup()
{
  byte idx;

  RtcRebootLoad();
  if (!RtcRebootValid()) { RtcReboot.fast_reboot_count = 0; }
  RtcReboot.fast_reboot_count++;
  RtcRebootSave();

  Serial.begin(baudrate);
  delay(10);
  Serial.println();
  seriallog_level = LOG_LEVEL_INFO;  // Allow specific serial messages until config loaded

  snprintf_P(my_version, sizeof(my_version), PSTR("%d.%d.%d"), VERSION >> 24 & 0xff, VERSION >> 16 & 0xff, VERSION >> 8 & 0xff);  // Release version 6.3.0
  if (VERSION & 0xff) {  // Development or patched version 6.3.0.10
    snprintf_P(my_version, sizeof(my_version), PSTR("%s.%d"), my_version, VERSION & 0xff);
  }
  char code_image[20];
  snprintf_P(my_image, sizeof(my_image), PSTR("(%s)"), GetTextIndexed(code_image, sizeof(code_image), CODE_IMAGE, kCodeImage));

  SettingsLoad();
  SettingsDelta();

  OsWatchInit();

  GetFeatures();

  if (1 == RtcReboot.fast_reboot_count) {  // Allow setting override only when all is well
    XdrvCall(FUNC_SETTINGS_OVERRIDE);
  }

  baudrate = Settings.baudrate * 1200;
  mdns_delayed_start = Settings.param[P_MDNS_DELAYED_START];
  seriallog_level = Settings.seriallog_level;
  seriallog_timer = SERIALLOG_TIMER;
#ifndef USE_EMULATION
  Settings.flag2.emulation = 0;
#endif  // USE_EMULATION
  syslog_level = Settings.syslog_level;
  stop_flash_rotate = Settings.flag.stop_flash_rotate;
  save_data_counter = Settings.save_data;
  sleep = Settings.sleep;

  // Disable functionality as possible cause of fast restart within BOOT_LOOP_TIME seconds (Exception, WDT or restarts)
  if (RtcReboot.fast_reboot_count > 1) {          // Restart twice
    Settings.flag3.user_esp8285_enable = 0;       // Disable ESP8285 Generic GPIOs interfering with flash SPI
    if (RtcReboot.fast_reboot_count > 2) {        // Restart 3 times
      for (byte i = 0; i < MAX_RULE_SETS; i++) {
        if (bitRead(Settings.rule_stop, i)) {
          bitWrite(Settings.rule_enabled, i, 0);  // Disable rules causing boot loop
        }
      }
    }
    if (RtcReboot.fast_reboot_count > 3) {        // Restarted 4 times
      Settings.rule_enabled = 0;                  // Disable all rules
    }
    if (RtcReboot.fast_reboot_count > 4) {        // Restarted 5 times
      Settings.module = SONOFF_BASIC;             // Reset module to Sonoff Basic
//      Settings.last_module = SONOFF_BASIC;
      for (byte i = 0; i < MAX_GPIO_PIN; i++) {
        Settings.my_gp.io[i] = GPIO_NONE;         // Reset user defined GPIO disabling sensors
      }
    }
    snprintf_P(log_data, sizeof(log_data), PSTR(D_LOG_APPLICATION D_LOG_SOME_SETTINGS_RESET " (%d)"), RtcReboot.fast_reboot_count);
    AddLog(LOG_LEVEL_DEBUG);
  }

  Settings.bootcount++;
  snprintf_P(log_data, sizeof(log_data), PSTR(D_LOG_APPLICATION D_BOOT_COUNT " %d"), Settings.bootcount);
  AddLog(LOG_LEVEL_DEBUG);

  Format(mqtt_client, Settings.mqtt_client, sizeof(mqtt_client));
  Format(mqtt_topic, Settings.mqtt_topic, sizeof(mqtt_topic));
  if (strstr(Settings.hostname, "%")) {
    strlcpy(Settings.hostname, WIFI_HOSTNAME, sizeof(Settings.hostname));
    snprintf_P(my_hostname, sizeof(my_hostname)-1, Settings.hostname, mqtt_topic, ESP.getChipId() & 0x1FFF);
  } else {
    snprintf_P(my_hostname, sizeof(my_hostname)-1, Settings.hostname);
  }

  GpioInit();

  SetSerialBaudrate(baudrate);

  WifiConnect();

  if (MOTOR == Settings.module) Settings.poweronstate = POWER_ALL_ON;  // Needs always on else in limbo!
  if (POWER_ALL_ALWAYS_ON == Settings.poweronstate) {
    SetDevicePower(1, SRC_RESTART);
  } else {
    if ((resetInfo.reason == REASON_DEFAULT_RST) || (resetInfo.reason == REASON_EXT_SYS_RST)) {
      switch (Settings.poweronstate) {
      case POWER_ALL_OFF:
      case POWER_ALL_OFF_PULSETIME_ON:
        power = 0;
        SetDevicePower(power, SRC_RESTART);
        break;
      case POWER_ALL_ON:  // All on
        power = (1 << devices_present) -1;
        SetDevicePower(power, SRC_RESTART);
        break;
      case POWER_ALL_SAVED_TOGGLE:
        power = (Settings.power & ((1 << devices_present) -1)) ^ POWER_MASK;
        if (Settings.flag.save_state) {
          SetDevicePower(power, SRC_RESTART);
        }
        break;
      case POWER_ALL_SAVED:
        power = Settings.power & ((1 << devices_present) -1);
        if (Settings.flag.save_state) {
          SetDevicePower(power, SRC_RESTART);
        }
        break;
      }
    } else {
      power = Settings.power & ((1 << devices_present) -1);
      if (Settings.flag.save_state) {
        SetDevicePower(power, SRC_RESTART);
      }
    }
  }

  // Issue #526 and #909
  for (byte i = 0; i < devices_present; i++) {
    if ((i < MAX_RELAYS) && (pin[GPIO_REL1 +i] < 99)) {
      bitWrite(power, i, digitalRead(pin[GPIO_REL1 +i]) ^ bitRead(rel_inverted, i));
    }
    if ((i < MAX_PULSETIMERS) && (bitRead(power, i) || (POWER_ALL_OFF_PULSETIME_ON == Settings.poweronstate))) {
      SetPulseTimer(i, Settings.pulse_timer[i]);
    }
  }
  blink_powersave = power;

  snprintf_P(log_data, sizeof(log_data), PSTR(D_PROJECT " %s %s (" D_CMND_TOPIC " %s, " D_FALLBACK " %s, " D_CMND_GROUPTOPIC " %s) " D_VERSION " %s%s-" ARDUINO_ESP8266_RELEASE),
    PROJECT, Settings.friendlyname[0], mqtt_topic, mqtt_client, Settings.mqtt_grptopic, my_version, my_image);
  AddLog(LOG_LEVEL_INFO);
#ifdef BE_MINIMAL
  snprintf_P(log_data, sizeof(log_data), PSTR(D_WARNING_MINIMAL_VERSION));
  AddLog(LOG_LEVEL_INFO);
#endif  // BE_MINIMAL

  RtcInit();

#ifdef USE_ARDUINO_OTA
  ArduinoOTAInit();
#endif  // USE_ARDUINO_OTA

  XdrvCall(FUNC_INIT);
  XsnsCall(FUNC_INIT);
}

void loop()
{
  XdrvCall(FUNC_LOOP);

  OsWatchLoop();

  if (TimeReached(button_debounce)) {
    SetNextTimeInterval(button_debounce, Settings.button_debounce);
    ButtonHandler();
  }
  if (TimeReached(switch_debounce)) {
    SetNextTimeInterval(switch_debounce, Settings.switch_debounce);
    SwitchHandler(0);
  }
  if (TimeReached(state_50msecond)) {
    SetNextTimeInterval(state_50msecond, 50);
    XdrvCall(FUNC_EVERY_50_MSECOND);
    XsnsCall(FUNC_EVERY_50_MSECOND);
  }
  if (TimeReached(state_100msecond)) {
    SetNextTimeInterval(state_100msecond, 100);
    Every100mSeconds();
    XdrvCall(FUNC_EVERY_100_MSECOND);
    XsnsCall(FUNC_EVERY_100_MSECOND);
  }
  if (TimeReached(state_250msecond)) {
    SetNextTimeInterval(state_250msecond, 250);
    Every250mSeconds();
    XdrvCall(FUNC_EVERY_250_MSECOND);
    XsnsCall(FUNC_EVERY_250_MSECOND);
  }

  if (!serial_local) SerialInput();

#ifdef USE_ARDUINO_OTA
  ArduinoOTA.handle();
  // Once OTA is triggered, only handle that and dont do other stuff. (otherwise it fails)
  while (arduino_ota_triggered) ArduinoOTA.handle();
#endif  // USE_ARDUINO_OTA

//  yield();     // yield == delay(0), delay contains yield, auto yield in loop
  delay(sleep);  // https://github.com/esp8266/Arduino/issues/2021
}
