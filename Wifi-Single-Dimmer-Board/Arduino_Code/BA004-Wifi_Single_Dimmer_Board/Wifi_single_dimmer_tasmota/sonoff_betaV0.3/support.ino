/*
  support.ino - support for Sonoff-Tasmota

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

IPAddress syslog_host_addr;      // Syslog host IP address
uint32_t syslog_host_hash = 0;   // Syslog host name hash

/*********************************************************************************************\
 * Watchdog extension (https://github.com/esp8266/Arduino/issues/1532)
\*********************************************************************************************/

Ticker tickerOSWatch;

#define OSWATCH_RESET_TIME 120

static unsigned long oswatch_last_loop_time;
byte oswatch_blocked_loop = 0;

#ifndef USE_WS2812_DMA  // Collides with Neopixelbus but solves exception
//void OsWatchTicker() ICACHE_RAM_ATTR;
#endif  // USE_WS2812_DMA

#ifdef USE_KNX
bool knx_started = false;
#endif  // USE_KNX

void OsWatchTicker()
{
  unsigned long t = millis();
  unsigned long last_run = abs(t - oswatch_last_loop_time);

#ifdef DEBUG_THEO
  snprintf_P(log_data, sizeof(log_data), PSTR(D_LOG_APPLICATION D_OSWATCH " FreeRam %d, rssi %d, last_run %d"), ESP.getFreeHeap(), WifiGetRssiAsQuality(WiFi.RSSI()), last_run);
  AddLog(LOG_LEVEL_DEBUG);
#endif  // DEBUG_THEO
  if (last_run >= (OSWATCH_RESET_TIME * 1000)) {
//    AddLog_P(LOG_LEVEL_INFO, PSTR(D_LOG_APPLICATION D_OSWATCH " " D_BLOCKED_LOOP ". " D_RESTARTING));  // Save iram space
    RtcSettings.oswatch_blocked_loop = 1;
    RtcSettingsSave();
//    ESP.restart();  // normal reboot
    ESP.reset();  // hard reset
  }
}

void OsWatchInit()
{
  oswatch_blocked_loop = RtcSettings.oswatch_blocked_loop;
  RtcSettings.oswatch_blocked_loop = 0;
  oswatch_last_loop_time = millis();
  tickerOSWatch.attach_ms(((OSWATCH_RESET_TIME / 3) * 1000), OsWatchTicker);
}

void OsWatchLoop()
{
  oswatch_last_loop_time = millis();
//  while(1) delay(1000);  // this will trigger the os watch
}

String GetResetReason()
{
  char buff[32];
  if (oswatch_blocked_loop) {
    strncpy_P(buff, PSTR(D_JSON_BLOCKED_LOOP), sizeof(buff));
    return String(buff);
  } else {
    return ESP.getResetReason();
  }
}

boolean OsWatchBlockedLoop()
{
  return oswatch_blocked_loop;
}
/*********************************************************************************************\
 * Miscellaneous
\*********************************************************************************************/

#ifdef ARDUINO_ESP8266_RELEASE_2_3_0
// Functions not available in 2.3.0

// http://clc-wiki.net/wiki/C_standard_library:string.h:memchr
void* memchr(const void* ptr, int value, size_t num)
{
  unsigned char *p = (unsigned char*)ptr;
  while (num--) {
    if (*p != (unsigned char)value) {
      p++;
    } else {
      return p;
    }
  }
  return 0;
}

// http://clc-wiki.net/wiki/C_standard_library:string.h:strspn
// Get span until any character in string
size_t strcspn(const char *str1, const char *str2)
{
  size_t ret = 0;
  while (*str1) {
    if (strchr(str2, *str1)) {  // Slow
      return ret;
    } else {
      str1++;
      ret++;
    }
  }
  return ret;
}
#endif  // ARDUINO_ESP8266_RELEASE_2_3_0

// Get span until single character in string
size_t strchrspn(const char *str1, int character)
{
  size_t ret = 0;
  char *start = (char*)str1;
  char *end = strchr(str1, character);
  if (end) ret = end - start;
  return ret;
}

// Function to return a substring defined by a delimiter at an index
char* subStr(char* dest, char* str, const char *delim, int index)
{
  char *act;
  char *sub;
  char *ptr;
  int i;

  // Since strtok consumes the first arg, make a copy
  strncpy(dest, str, strlen(str)+1);
  for (i = 1, act = dest; i <= index; i++, act = NULL) {
    sub = strtok_r(act, delim, &ptr);
    if (sub == NULL) break;
  }
  sub = Trim(sub);
  return sub;
}

double CharToDouble(char *str)
{
  // simple ascii to double, because atof or strtod are too large
  char strbuf[24];

  strlcpy(strbuf, str, sizeof(strbuf));
  char *pt;
  double left = atoi(strbuf);
  double right = 0;
  short len = 0;
  pt = strtok (strbuf, ".");
  if (pt) {
    pt = strtok (NULL, ".");
    if (pt) {
      right = atoi(pt);
      len = strlen(pt);
      double fac = 1;
      while (len) {
        fac /= 10.0;
        len--;
      }
      // pow is also very large
      //double fac=pow(10,-len);
      right *= fac;
    }
  }
  double result = left + right;
  if (left < 0) { result = left - right; }
  return result;
}

int TextToInt(char *str)
{
  char *p;
  uint8_t radix = 10;
  if ('#' == str[0]) {
    radix = 16;
    str++;
  }
  return strtol(str, &p, radix);
}

char* dtostrfd(double number, unsigned char prec, char *s)
{
  if ((isnan(number)) || (isinf(number))) {  // Fix for JSON output (https://stackoverflow.com/questions/1423081/json-left-out-infinity-and-nan-json-status-in-ecmascript)
    strcpy(s, "null");
    return s;
  } else {
    return dtostrf(number, 1, prec, s);
  }
}

char* Unescape(char* buffer, uint16_t* size)
{
  uint8_t* read = (uint8_t*)buffer;
  uint8_t* write = (uint8_t*)buffer;
  uint16_t start_size = *size;
  uint16_t end_size = *size;
  uint8_t che = 0;

  while (start_size > 0) {
    uint8_t ch = *read++;
    start_size--;
    if (ch != '\\') {
      *write++ = ch;
    } else {
      if (start_size > 0) {
        uint8_t chi = *read++;
        start_size--;
        end_size--;
        switch (chi) {
          case '\\': che = '\\'; break;  // 5C Backslash
          case 'a': che = '\a'; break;   // 07 Bell (Alert)
          case 'b': che = '\b'; break;   // 08 Backspace
          case 'e': che = '\e'; break;   // 1B Escape
          case 'f': che = '\f'; break;   // 0C Formfeed
          case 'n': che = '\n'; break;   // 0A Linefeed (Newline)
          case 'r': che = '\r'; break;   // 0D Carriage return
          case 's': che = ' ';  break;   // 20 Space
          case 't': che = '\t'; break;   // 09 Horizontal tab
          case 'v': che = '\v'; break;   // 0B Vertical tab
//          case '?': che = '\?'; break;   // 3F Question mark
          default : {
            che = chi;
            *write++ = ch;
            end_size++;
          }
        }
        *write++ = che;
      }
    }
  }
  *size = end_size;
  return buffer;
}

char* RemoveSpace(char* p)
{
  char* write = p;
  char* read = p;
  char ch = '.';

  while (ch != '\0') {
    ch = *read++;
    if (!isspace(ch)) {
      *write++ = ch;
    }
  }
  *write = '\0';
  return p;
}

char* UpperCase(char* dest, const char* source)
{
  char* write = dest;
  const char* read = source;
  char ch = '.';

  while (ch != '\0') {
    ch = *read++;
    *write++ = toupper(ch);
  }
  return dest;
}

char* UpperCase_P(char* dest, const char* source)
{
  char* write = dest;
  const char* read = source;
  char ch = '.';

  while (ch != '\0') {
    ch = pgm_read_byte(read++);
    *write++ = toupper(ch);
  }
  return dest;
}

char* LTrim(char* p)
{
  while ((*p != '\0') && (isblank(*p))) {
    p++;                                     // Trim leading spaces
  }
  return p;
}

char* RTrim(char* p)
{
  char* q = p + strlen(p) -1;
  while ((q >= p) && (isblank(*q))) {
    q--;                                     // Trim trailing spaces
  }
  q++;
  *q = '\0';
  return p;
}

char* Trim(char* p)
{
  if (*p == '\0') { return p; }
  while (isspace(*p)) { p++; }            // Trim leading spaces
  if (*p == '\0') { return p; }
  char* q = p + strlen(p) -1;
  while (isspace(*q) && q >= p) { q--; }  // Trim trailing spaces
  q++;
  *q = '\0';
  return p;
}

char* NoAlNumToUnderscore(char* dest, const char* source)
{
  char* write = dest;
  const char* read = source;
  char ch = '.';

  while (ch != '\0') {
    ch = *read++;
    *write++ = (isalnum(ch) || ('\0' == ch)) ? ch : '_';
  }
  return dest;
}

void SetShortcut(char* str, uint8_t action)
{
  if ('\0' != str[0]) {     // There must be at least one character in the buffer
    str[0] = '0' + action;  // SC_CLEAR, SC_DEFAULT, SC_USER
    str[1] = '\0';
  }
}

uint8_t Shortcut(const char* str)
{
  uint8_t result = 10;

  if ('\0' == str[1]) {    // Only allow single character input for shortcut
    if (('"' == str[0]) || ('0' == str[0])) {
      result = SC_CLEAR;
    } else {
      result = atoi(str);  // 1 = SC_DEFAULT, 2 = SC_USER
      if (0 == result) {
        result = 10;
      }
    }
  }
  return result;
}

boolean ParseIp(uint32_t* addr, const char* str)
{
  uint8_t *part = (uint8_t*)addr;
  byte i;

  *addr = 0;
  for (i = 0; i < 4; i++) {
    part[i] = strtoul(str, NULL, 10);        // Convert byte
    str = strchr(str, '.');
    if (str == NULL || *str == '\0') {
      break;  // No more separators, exit
    }
    str++;                                   // Point to next character after separator
  }
  return (3 == i);
}

void MakeValidMqtt(byte option, char* str)
{
// option 0 = replace by underscore
// option 1 = delete character
  uint16_t i = 0;
  while (str[i] > 0) {
//        if ((str[i] == '/') || (str[i] == '+') || (str[i] == '#') || (str[i] == ' ')) {
    if ((str[i] == '+') || (str[i] == '#') || (str[i] == ' ')) {
      if (option) {
        uint16_t j = i;
        while (str[j] > 0) {
          str[j] = str[j +1];
          j++;
        }
        i--;
      } else {
        str[i] = '_';
      }
    }
    i++;
  }
}

// Function to parse & check if version_str is newer than our currently installed version.
bool NewerVersion(char* version_str)
{
  uint32_t version = 0;
  uint8_t i = 0;
  char *str_ptr;
  char* version_dup = strdup(version_str);  // Duplicate the version_str as strtok_r will modify it.

  if (!version_dup) {
    return false;  // Bail if we can't duplicate. Assume bad.
  }
  // Loop through the version string, splitting on '.' seperators.
  for (char *str = strtok_r(version_dup, ".", &str_ptr); str && i < sizeof(VERSION); str = strtok_r(NULL, ".", &str_ptr), i++) {
    int field = atoi(str);
    // The fields in a version string can only range from 0-255.
    if ((field < 0) || (field > 255)) {
      free(version_dup);
      return false;
    }
    // Shuffle the accumulated bytes across, and add the new byte.
    version = (version << 8) + field;
    // Check alpha delimiter after 1.2.3 only
    if ((2 == i) && isalpha(str[strlen(str)-1])) {
      field = str[strlen(str)-1] & 0x1f;
      version = (version << 8) + field;
      i++;
    }
  }
  free(version_dup);  // We no longer need this.
  // A version string should have 2-4 fields. e.g. 1.2, 1.2.3, or 1.2.3a (= 1.2.3.1).
  // If not, then don't consider it a valid version string.
  if ((i < 2) || (i > sizeof(VERSION))) {
    return false;
  }
  // Keep shifting the parsed version until we hit the maximum number of tokens.
  // VERSION stores the major number of the version in the most significant byte of the uint32_t.
  while (i < sizeof(VERSION)) {
    version <<= 8;
    i++;
  }
  // Now we should have a fully constructed version number in uint32_t form.
  return (version > VERSION);
}

char* GetPowerDevice(char* dest, uint8_t idx, size_t size, uint8_t option)
{
  char sidx[8];

  strncpy_P(dest, S_RSLT_POWER, size);                // POWER
  if ((devices_present + option) > 1) {
    snprintf_P(sidx, sizeof(sidx), PSTR("%d"), idx);  // x
    strncat(dest, sidx, size);                        // POWERx
  }
  return dest;
}

char* GetPowerDevice(char* dest, uint8_t idx, size_t size)
{
  return GetPowerDevice(dest, idx, size, 0);
}

float ConvertTemp(float c)
{
  float result = c;

  if (!isnan(c) && Settings.flag.temperature_conversion) {
    result = c * 1.8 + 32;  // Fahrenheit
  }
  return result;
}

char TempUnit()
{
  return (Settings.flag.temperature_conversion) ? 'F' : 'C';
}

float ConvertPressure(float p)
{
  float result = p;

  if (!isnan(p) && Settings.flag.pressure_conversion) {
    result = p * 0.75006375541921;  // mmHg
  }
  return result;
}

String PressureUnit()
{
  return (Settings.flag.pressure_conversion) ? String(D_UNIT_MILLIMETER_MERCURY) : String(D_UNIT_PRESSURE);
}

void SetGlobalValues(float temperature, float humidity)
{
  global_update = uptime;
  global_temperature = temperature;
  global_humidity = humidity;
}

void ResetGlobalValues()
{
  if ((uptime - global_update) > GLOBAL_VALUES_VALID) {  // Reset after 5 minutes
    global_update = 0;
    global_temperature = 0;
    global_humidity = 0;
  }
}

double FastPrecisePow(double a, double b)
{
  // https://martin.ankerl.com/2012/01/25/optimized-approximative-pow-in-c-and-cpp/
  // calculate approximation with fraction of the exponent
  int e = (int)b;
  union {
    double d;
    int x[2];
  } u = { a };
  u.x[1] = (int)((b - e) * (u.x[1] - 1072632447) + 1072632447);
  u.x[0] = 0;
  // exponentiation by squaring with the exponent's integer part
  // double r = u.d makes everything much slower, not sure why
  double r = 1.0;
  while (e) {
    if (e & 1) {
      r *= a;
    }
    a *= a;
    e >>= 1;
  }
  return r * u.d;
}

uint32_t SqrtInt(uint32_t num)
{
  if (num <= 1) {
    return num;
  }

  uint32_t x = num / 2;
  uint32_t y;
  do {
    y = (x + num / x) / 2;
    if (y >= x) {
      return x;
    }
    x = y;
  } while (true);
}

uint32_t RoundSqrtInt(uint32_t num)
{
  uint32_t s = SqrtInt(4 * num);
  if (s & 1) {
    s++;
  }
  return s / 2;
}

char* GetTextIndexed(char* destination, size_t destination_size, uint16_t index, const char* haystack)
{
  // Returns empty string if not found
  // Returns text of found
  char* write = destination;
  const char* read = haystack;

  index++;
  while (index--) {
    size_t size = destination_size -1;
    write = destination;
    char ch = '.';
    while ((ch != '\0') && (ch != '|')) {
      ch = pgm_read_byte(read++);
      if (size && (ch != '|'))  {
        *write++ = ch;
        size--;
      }
    }
    if (0 == ch) {
      if (index) {
        write = destination;
      }
      break;
    }
  }
  *write = '\0';
  return destination;
}

int GetCommandCode(char* destination, size_t destination_size, const char* needle, const char* haystack)
{
  // Returns -1 of not found
  // Returns index and command if found
  int result = -1;
  const char* read = haystack;
  char* write = destination;

  while (true) {
    result++;
    size_t size = destination_size -1;
    write = destination;
    char ch = '.';
    while ((ch != '\0') && (ch != '|')) {
      ch = pgm_read_byte(read++);
      if (size && (ch != '|'))  {
        *write++ = ch;
        size--;
      }
    }
    *write = '\0';
    if (!strcasecmp(needle, destination)) {
      break;
    }
    if (0 == ch) {
      result = -1;
      break;
    }
  }
  return result;
}

int GetStateNumber(char *state_text)
{
  char command[CMDSZ];
  int state_number = -1;

  if (GetCommandCode(command, sizeof(command), state_text, kOptionOff) >= 0) {
    state_number = 0;
  }
  else if (GetCommandCode(command, sizeof(command), state_text, kOptionOn) >= 0) {
    state_number = 1;
  }
  else if (GetCommandCode(command, sizeof(command), state_text, kOptionToggle) >= 0) {
    state_number = 2;
  }
  else if (GetCommandCode(command, sizeof(command), state_text, kOptionBlink) >= 0) {
    state_number = 3;
  }
  else if (GetCommandCode(command, sizeof(command), state_text, kOptionBlinkOff) >= 0) {
    state_number = 4;
  }
  return state_number;
}

boolean GetUsedInModule(byte val, uint8_t *arr)
{
  int offset = 0;

  if (!val) { return false; }  // None

  if ((val >= GPIO_KEY1) && (val < GPIO_KEY1 + MAX_KEYS)) {
    offset = (GPIO_KEY1_NP - GPIO_KEY1);
  }
  if ((val >= GPIO_KEY1_NP) && (val < GPIO_KEY1_NP + MAX_KEYS)) {
    offset = -(GPIO_KEY1_NP - GPIO_KEY1);
  }

  if ((val >= GPIO_SWT1) && (val < GPIO_SWT1 + MAX_SWITCHES)) {
    offset = (GPIO_SWT1_NP - GPIO_SWT1);
  }
  if ((val >= GPIO_SWT1_NP) && (val < GPIO_SWT1_NP + MAX_SWITCHES)) {
    offset = -(GPIO_SWT1_NP - GPIO_SWT1);
  }

  if ((val >= GPIO_REL1) && (val < GPIO_REL1 + MAX_RELAYS)) {
    offset = (GPIO_REL1_INV - GPIO_REL1);
  }
  if ((val >= GPIO_REL1_INV) && (val < GPIO_REL1_INV + MAX_RELAYS)) {
    offset = -(GPIO_REL1_INV - GPIO_REL1);
  }

  if ((val >= GPIO_LED1) && (val < GPIO_LED1 + MAX_LEDS)) {
    offset = (GPIO_LED1_INV - GPIO_LED1);
  }
  if ((val >= GPIO_LED1_INV) && (val < GPIO_LED1_INV + MAX_LEDS)) {
    offset = -(GPIO_LED1_INV - GPIO_LED1);
  }

  if ((val >= GPIO_PWM1) && (val < GPIO_PWM1 + MAX_PWMS)) {
    offset = (GPIO_PWM1_INV - GPIO_PWM1);
  }
  if ((val >= GPIO_PWM1_INV) && (val < GPIO_PWM1_INV + MAX_PWMS)) {
    offset = -(GPIO_PWM1_INV - GPIO_PWM1);
  }

  if ((val >= GPIO_CNTR1) && (val < GPIO_CNTR1 + MAX_COUNTERS)) {
    offset = (GPIO_CNTR1_NP - GPIO_CNTR1);
  }
  if ((val >= GPIO_CNTR1_NP) && (val < GPIO_CNTR1_NP + MAX_COUNTERS)) {
    offset = -(GPIO_CNTR1_NP - GPIO_CNTR1);
  }

  for (byte i = 0; i < MAX_GPIO_PIN; i++) {
    if (arr[i] == val) { return true; }
    if (arr[i] == val + offset) { return true; }
  }
  return false;
}

void SetSerialBaudrate(int baudrate)
{
  Settings.baudrate = baudrate / 1200;
  if (Serial.baudRate() != baudrate) {
    if (seriallog_level) {
      snprintf_P(log_data, sizeof(log_data), PSTR(D_LOG_APPLICATION D_SET_BAUDRATE_TO " %d"), baudrate);
      AddLog(LOG_LEVEL_INFO);
    }
    delay(100);
    Serial.flush();
    Serial.begin(baudrate, serial_config);
    delay(10);
    Serial.println();
  }
}

void ClaimSerial()
{
  serial_local = 1;
  AddLog_P(LOG_LEVEL_INFO, PSTR("SNS: Hardware Serial"));
  SetSeriallog(LOG_LEVEL_NONE);
  baudrate = Serial.baudRate();
  Settings.baudrate = baudrate / 1200;
}

void SerialSendRaw(char *codes)
{
  char *p;
  char stemp[3];
  uint8_t code;

  int size = strlen(codes);

  while (size > 0) {
    snprintf(stemp, sizeof(stemp), codes);
    code = strtol(stemp, &p, 16);
    Serial.write(code);
    size -= 2;
    codes += 2;
  }
}

uint32_t GetHash(const char *buffer, size_t size)
{
  uint32_t hash = 0;
  for (uint16_t i = 0; i <= size; i++) {
    hash += (uint8_t)*buffer++ * (i +1);
  }
  return hash;
}

void ShowSource(int source)
{
  if ((source > 0) && (source < SRC_MAX)) {
    char stemp1[20];
    snprintf_P(log_data, sizeof(log_data), PSTR("SRC: %s"), GetTextIndexed(stemp1, sizeof(stemp1), source, kCommandSource));
    AddLog(LOG_LEVEL_DEBUG);
  }
}

uint8_t ValidGPIO(uint8_t pin, uint8_t gpio)
{
  uint8_t result = gpio;
  if ((WEMOS == Settings.module) && (!Settings.flag3.user_esp8285_enable)) {
    if ((pin == 9) || (pin == 10)) { result = GPIO_NONE; }  // Disable possible flash GPIO9 and GPIO10
  }
  return result;
}

/*********************************************************************************************\
 * Sleep aware time scheduler functions borrowed from ESPEasy
\*********************************************************************************************/

long TimeDifference(unsigned long prev, unsigned long next)
{
  // Return the time difference as a signed value, taking into account the timers may overflow.
  // Returned timediff is between -24.9 days and +24.9 days.
  // Returned value is positive when "next" is after "prev"
  long signed_diff = 0;
  // To cast a value to a signed long, the difference may not exceed half 0xffffffffUL (= 4294967294)
  const unsigned long half_max_unsigned_long = 2147483647u;  // = 2^31 -1
  if (next >= prev) {
    const unsigned long diff = next - prev;
    if (diff <= half_max_unsigned_long) {                    // Normal situation, just return the difference.
      signed_diff = static_cast<long>(diff);                 // Difference is a positive value.
    } else {
      // prev has overflow, return a negative difference value
      signed_diff = static_cast<long>((0xffffffffUL - next) + prev + 1u);
      signed_diff = -1 * signed_diff;
    }
  } else {
    // next < prev
    const unsigned long diff = prev - next;
    if (diff <= half_max_unsigned_long) {                    // Normal situation, return a negative difference value
      signed_diff = static_cast<long>(diff);
      signed_diff = -1 * signed_diff;
    } else {
      // next has overflow, return a positive difference value
      signed_diff = static_cast<long>((0xffffffffUL - prev) + next + 1u);
    }
  }
  return signed_diff;
}

long TimePassedSince(unsigned long timestamp)
{
  // Compute the number of milliSeconds passed since timestamp given.
  // Note: value can be negative if the timestamp has not yet been reached.
  return TimeDifference(timestamp, millis());
}

bool TimeReached(unsigned long timer)
{
  // Check if a certain timeout has been reached.
  const long passed = TimePassedSince(timer);
  return (passed >= 0);
}

void SetNextTimeInterval(unsigned long& timer, const unsigned long step)
{
  timer += step;
  const long passed = TimePassedSince(timer);
  if (passed < 0) { return; }   // Event has not yet happened, which is fine.
  if (static_cast<unsigned long>(passed) > step) {
    // No need to keep running behind, start again.
    timer = millis() + step;
    return;
  }
  // Try to get in sync again.
  timer = millis() + (step - passed);
}

/*********************************************************************************************\
 * Fill feature list
\*********************************************************************************************/

void GetFeatures()
{
  feature_drv1 = 0x00000000;   // xdrv_01_mqtt.ino, xdrv_01_light.ino, xdrv_04_snfbridge.ino

//  feature_drv1 |= 0x00000001;
//  feature_drv1 |= 0x00000002;

#ifdef USE_I2C
  feature_drv1 |= 0x00000004;  // sonoff.ino
#endif
#ifdef USE_SPI
  feature_drv1 |= 0x00000008;  // sonoff.ino
#endif
#ifdef USE_DISCOVERY
  feature_drv1 |= 0x00000010;  // sonoff.ino
#endif
#ifdef USE_ARDUINO_OTA
  feature_drv1 |= 0x00000020;  // sonoff.ino
#endif
#ifdef USE_MQTT_TLS
  feature_drv1 |= 0x00000040;  // sonoff.ino
#endif
#ifdef USE_WEBSERVER
  feature_drv1 |= 0x00000080;  // xdrv_02_webserver.ino
#endif
#ifdef WEBSERVER_ADVERTISE
  feature_drv1 |= 0x00000100;  // xdrv_02_webserver.ino
#endif
#ifdef USE_EMULATION
  feature_drv1 |= 0x00000200;  // xplg_wemohue.ino
#endif
#if (MQTT_LIBRARY_TYPE == MQTT_PUBSUBCLIENT)
  feature_drv1 |= 0x00000400;  // xdrv_01_mqtt.ino
#endif
#if (MQTT_LIBRARY_TYPE == MQTT_TASMOTAMQTT)
  feature_drv1 |= 0x00000800;  // xdrv_01_mqtt.ino
#endif
#if (MQTT_LIBRARY_TYPE == MQTT_ESPMQTTARDUINO)      // Obsolete since 6.2.1.11
  feature_drv1 |= 0x00001000;  // xdrv_01_mqtt.ino
#endif
#ifdef MQTT_HOST_DISCOVERY
  feature_drv1 |= 0x00002000;  // xdrv_01_mqtt.ino
#endif
#ifdef USE_ARILUX_RF
  feature_drv1 |= 0x00004000;  // xdrv_04_light.ino
#endif
#ifdef USE_WS2812
  feature_drv1 |= 0x00008000;  // xdrv_04_light.ino
#endif
#ifdef USE_WS2812_DMA
  feature_drv1 |= 0x00010000;  // xdrv_04_light.ino
#endif
#ifdef USE_IR_REMOTE
  feature_drv1 |= 0x00020000;  // xdrv_05_irremote.ino
#endif
#ifdef USE_IR_HVAC
  feature_drv1 |= 0x00040000;  // xdrv_05_irremote.ino
#endif
#ifdef USE_IR_RECEIVE
  feature_drv1 |= 0x00080000;  // xdrv_05_irremote.ino
#endif
#ifdef USE_DOMOTICZ
  feature_drv1 |= 0x00100000;  // xdrv_07_domoticz.ino
#endif
#ifdef USE_DISPLAY
  feature_drv1 |= 0x00200000;  // xdrv_13_display.ino
#endif
#ifdef USE_HOME_ASSISTANT
  feature_drv1 |= 0x00400000;  // xdrv_12_home_assistant.ino
#endif
#ifdef USE_SERIAL_BRIDGE
  feature_drv1 |= 0x00800000;  // xdrv_08_serial_bridge.ino
#endif
#ifdef USE_TIMERS
  feature_drv1 |= 0x01000000;  // xdrv_09_timers.ino
#endif
#ifdef USE_SUNRISE
  feature_drv1 |= 0x02000000;  // xdrv_09_timers.ino
#endif
#ifdef USE_TIMERS_WEB
  feature_drv1 |= 0x04000000;  // xdrv_09_timers.ino
#endif
#ifdef USE_RULES
  feature_drv1 |= 0x08000000;  // xdrv_10_rules.ino
#endif
#ifdef USE_KNX
  feature_drv1 |= 0x10000000;  // xdrv_11_knx.ino
#endif
#ifdef USE_WPS
  feature_drv1 |= 0x20000000;  // support.ino
#endif
#ifdef USE_SMARTCONFIG
  feature_drv1 |= 0x40000000;  // support.ino
#endif
#if (MQTT_LIBRARY_TYPE == MQTT_ARDUINOMQTT)
  feature_drv1 |= 0x80000000;  // xdrv_01_mqtt.ino
#endif

/*********************************************************************************************/

  feature_drv2 = 0x00000000;

#ifdef USE_CONFIG_OVERRIDE
  feature_drv2 |= 0x00000001;  // user_config(_override).h
#endif
#ifdef BE_MINIMAL
  feature_drv2 |= 0x00000002;  // user_config(_override).h
#endif
#ifdef USE_SENSORS
  feature_drv2 |= 0x00000004;  // user_config(_override).h
#endif
#ifdef USE_CLASSIC
  feature_drv2 |= 0x00000008;  // user_config(_override).h
#endif
#ifdef USE_KNX_NO_EMULATION
  feature_drv2 |= 0x00000010;  // user_config(_override).h
#endif
#ifdef USE_DISPLAY_MODES1TO5
  feature_drv2 |= 0x00000020;  // xdrv_13_display.ino
#endif
#ifdef USE_DISPLAY_GRAPH
  feature_drv2 |= 0x00000040;  // xdrv_13_display.ino
#endif
#ifdef USE_DISPLAY_LCD
  feature_drv2 |= 0x00000080;  // xdsp_01_lcd.ino
#endif
#ifdef USE_DISPLAY_SSD1306
  feature_drv2 |= 0x00000100;  // xdsp_02_ssd1306.ino
#endif
#ifdef USE_DISPLAY_MATRIX
  feature_drv2 |= 0x00000200;  // xdsp_03_matrix.ino
#endif
#ifdef USE_DISPLAY_ILI9341
  feature_drv2 |= 0x00000400;  // xdsp_04_ili9341.ino
#endif
#ifdef USE_DISPLAY_EPAPER
  feature_drv2 |= 0x00000800;  // xdsp_05_epaper.ino
#endif
#ifdef USE_DISPLAY_SH1106
  feature_drv2 |= 0x00001000;  // xdsp_06_sh1106.ino
#endif
#ifdef USE_MP3_PLAYER
  feature_drv2 |= 0x00002000;  // xdrv_14_mp3.ino
#endif
#ifdef USE_PCA9685
  feature_drv2 |= 0x00004000;  // xdrv_15_pca9685.ino
#endif
#ifdef USE_TUYA_DIMMER
  feature_drv2 |= 0x00008000;  // xdrv_16_tuyadimmer.ino
#endif
#ifdef USE_RC_SWITCH
  feature_drv2 |= 0x00010000;  // xdrv_17_rcswitch.ino
#endif
#ifdef USE_ARMTRONIX_DIMMERS
  feature_drv2 |= 0x00020000;  // xdrv_18_armtronixdimmer.ino
#endif

#ifdef USE_ARMTRONIX_SINGLE_DIMMER
  feature_drv2 |= 0x00030000;  // xdrv_19_armtronixdimmer.ino
#endif


#ifdef NO_EXTRA_4K_HEAP
  feature_drv2 |= 0x00800000;  // sonoff_post.h
#endif
#ifdef VTABLES_IN_IRAM
  feature_drv2 |= 0x01000000;  // platformio.ini
#endif
#ifdef VTABLES_IN_DRAM
  feature_drv2 |= 0x02000000;  // platformio.ini
#endif
#ifdef VTABLES_IN_FLASH
  feature_drv2 |= 0x04000000;  // platformio.ini
#endif
#ifdef PIO_FRAMEWORK_ARDUINO_LWIP_HIGHER_BANDWIDTH
  feature_drv2 |= 0x08000000;  // platformio.ini
#endif
#ifdef PIO_FRAMEWORK_ARDUINO_LWIP2_LOW_MEMORY
  feature_drv2 |= 0x10000000;  // platformio.ini
#endif
#ifdef PIO_FRAMEWORK_ARDUINO_LWIP2_HIGHER_BANDWIDTH
  feature_drv2 |= 0x20000000;  // platformio.ini
#endif
#ifdef DEBUG_THEO
  feature_drv2 |= 0x40000000;  // xdrv_99_debug.ino
#endif
#ifdef USE_DEBUG_DRIVER
  feature_drv2 |= 0x80000000;  // xdrv_99_debug.ino
#endif

/*********************************************************************************************/

  feature_sns1 = 0x00000000;   // xsns_01_counter.ino, xsns_04_snfsc.ino

//  feature_sns1 |= 0x00000001;

#ifdef USE_ADC_VCC
  feature_sns1 |= 0x00000002;  // support.ino (ADC)
#endif
#ifdef USE_ENERGY_SENSOR
  feature_sns1 |= 0x00000004;  // xdrv_03_energy.ino
#endif
#ifdef USE_PZEM004T
  feature_sns1 |= 0x00000008;  // xnrg_03_pzem004t.ino
#endif
#ifdef USE_DS18B20
  feature_sns1 |= 0x00000010;  // xsns_05_ds18b20.ino
#endif
#ifdef USE_DS18x20_LEGACY
  feature_sns1 |= 0x00000020;  // xsns_05_ds18x20_legacy.ino
#endif
#ifdef USE_DS18x20
  feature_sns1 |= 0x00000040;  // xsns_05_ds18x20.ino
#endif
#ifdef USE_DHT
  feature_sns1 |= 0x00000080;  // xsns_06_dht.ino
#endif
#ifdef USE_SHT
  feature_sns1 |= 0x00000100;  // xsns_07_sht1x.ino
#endif
#ifdef USE_HTU
  feature_sns1 |= 0x00000200;  // xsns_08_htu21.ino
#endif
#ifdef USE_BMP
  feature_sns1 |= 0x00000400;  // xsns_09_bmp.ino
#endif
#ifdef USE_BME680
  feature_sns1 |= 0x00000800;  // xsns_09_bmp.ino - BME680
#endif
#ifdef USE_BH1750
  feature_sns1 |= 0x00001000;  // xsns_10_bh1750.ino
#endif
#ifdef USE_VEML6070
  feature_sns1 |= 0x00002000;  // xsns_11_veml6070.ino
#endif
#ifdef USE_ADS1115_I2CDEV
  feature_sns1 |= 0x00004000;  // xsns_12_ads1115_i2cdev.ino
#endif
#ifdef USE_ADS1115
  feature_sns1 |= 0x00008000;  // xsns_12_ads1115.ino
#endif
#ifdef USE_INA219
  feature_sns1 |= 0x00010000;  // xsns_13_ina219.ino
#endif
#ifdef USE_SHT3X
  feature_sns1 |= 0x00020000;  // xsns_14_sht3x.ino
#endif
#ifdef USE_MHZ19
  feature_sns1 |= 0x00040000;  // xsns_15_mhz19.ino
#endif
#ifdef USE_TSL2561
  feature_sns1 |= 0x00080000;  // xsns_16_tsl2561.ino
#endif
#ifdef USE_SENSEAIR
  feature_sns1 |= 0x00100000;  // xsns_17_senseair.ino
#endif
#ifdef USE_PMS5003
  feature_sns1 |= 0x00200000;  // xsns_18_pms5003.ino
#endif
#ifdef USE_MGS
  feature_sns1 |= 0x00400000;  // xsns_19_mgs.ino
#endif
#ifdef USE_NOVA_SDS
  feature_sns1 |= 0x00800000;  // xsns_20_novasds.ino
#endif
#ifdef USE_SGP30
  feature_sns1 |= 0x01000000;  // xsns_21_sgp30.ino
#endif
#ifdef USE_SR04
  feature_sns1 |= 0x02000000;  // xsns_22_sr04.ino
#endif
#ifdef USE_SDM120
  feature_sns1 |= 0x04000000;  // xsns_23_sdm120.ino
#endif
#ifdef USE_SI1145
  feature_sns1 |= 0x08000000;  // xsns_24_si1145.ino
#endif
#ifdef USE_SDM630
  feature_sns1 |= 0x10000000;  // xsns_25_sdm630.ino
#endif
#ifdef USE_LM75AD
  feature_sns1 |= 0x20000000;  // xsns_26_lm75ad.ino
#endif
#ifdef USE_APDS9960
  feature_sns1 |= 0x40000000;  // xsns_27_apds9960.ino
#endif
#ifdef USE_TM1638
  feature_sns1 |= 0x80000000;  // xsns_28_tm1638.ino
#endif

/*********************************************************************************************/

  feature_sns2 = 0x00000000;

#ifdef USE_MCP230xx
  feature_sns2 |= 0x00000001;  // xsns_29_mcp230xx.ino
#endif
#ifdef USE_MPR121
  feature_sns2 |= 0x00000002;  // xsns_30_mpr121.ino
#endif
#ifdef USE_CCS811
  feature_sns2 |= 0x00000004;  // xsns_31_ccs811.ino
#endif
#ifdef USE_MPU6050
  feature_sns2 |= 0x00000008;  // xsns_32_mpu6050.ino
#endif
#ifdef USE_MCP230xx_OUTPUT
  feature_sns2 |= 0x00000010;  // xsns_29_mcp230xx.ino
#endif
#ifdef USE_MCP230xx_DISPLAYOUTPUT
  feature_sns2 |= 0x00000020;  // xsns_29_mcp230xx.ino
#endif
#ifdef USE_HLW8012
  feature_sns2 |= 0x00000040;  // xnrg_01_hlw8012.ino
#endif
#ifdef USE_CSE7766
  feature_sns2 |= 0x00000080;  // xnrg_02_cse7766.ino
#endif
#ifdef USE_MCP39F501
  feature_sns2 |= 0x00000100;  // xnrg_04_mcp39f501.ino
#endif
#ifdef USE_PZEM_AC
  feature_sns2 |= 0x00000200;  // xnrg_05_pzem_ac.ino
#endif
#ifdef USE_DS3231
  feature_sns2 |= 0x00000400;  // xsns_33_ds3231.ino
#endif
#ifdef USE_HX711
  feature_sns2 |= 0x00000800;  // xsns_34_hx711.ino
#endif
#ifdef USE_PZEM_DC
  feature_sns2 |= 0x00001000;  // xnrg_06_pzem_dc.ino
#endif
#ifdef USE_TX20_WIND_SENSOR
  feature_sns2 |= 0x00002000;  // xsns_35_tx20.ino
#endif



}

/*********************************************************************************************\
 * Wifi
\*********************************************************************************************/

#define WIFI_CONFIG_SEC        180  // seconds before restart
#define WIFI_CHECK_SEC         20   // seconds
#define WIFI_RETRY_OFFSET_SEC  20   // seconds

uint8_t wifi_counter;
uint8_t wifi_retry_init;
uint8_t wifi_retry;
uint8_t wifi_status;
uint8_t wps_result;
uint8_t wifi_config_type = 0;
uint8_t wifi_config_counter = 0;

int WifiGetRssiAsQuality(int rssi)
{
  int quality = 0;

  if (rssi <= -100) {
    quality = 0;
  } else if (rssi >= -50) {
    quality = 100;
  } else {
    quality = 2 * (rssi + 100);
  }
  return quality;
}

boolean WifiConfigCounter()
{
  if (wifi_config_counter) {
    wifi_config_counter = WIFI_CONFIG_SEC;
  }
  return (wifi_config_counter);
}

extern "C" {
#include "user_interface.h"
}

void WifiWpsStatusCallback(wps_cb_status status);

void WifiWpsStatusCallback(wps_cb_status status)
{
/* from user_interface.h:
  enum wps_cb_status {
    WPS_CB_ST_SUCCESS = 0,
    WPS_CB_ST_FAILED,
    WPS_CB_ST_TIMEOUT,
    WPS_CB_ST_WEP,      // WPS failed because that WEP is not supported
    WPS_CB_ST_SCAN_ERR, // can not find the target WPS AP
  };
*/
  wps_result = status;
  if (WPS_CB_ST_SUCCESS == wps_result) {
    wifi_wps_disable();
  } else {
    snprintf_P(log_data, sizeof(log_data), PSTR(D_LOG_WIFI D_WPS_FAILED_WITH_STATUS " %d"), wps_result);
    AddLog(LOG_LEVEL_DEBUG);
    wifi_config_counter = 2;
  }
}

boolean WifiWpsConfigDone(void)
{
  return (!wps_result);
}

boolean WifiWpsConfigBegin(void)
{
  wps_result = 99;
  if (!wifi_wps_disable()) { return false; }
  if (!wifi_wps_enable(WPS_TYPE_PBC)) { return false; }  // so far only WPS_TYPE_PBC is supported (SDK 2.0.0)
  if (!wifi_set_wps_cb((wps_st_cb_t) &WifiWpsStatusCallback)) { return false; }
  if (!wifi_wps_start()) { return false; }
  return true;
}

void WifiConfig(uint8_t type)
{
  if (!wifi_config_type) {
    if ((WIFI_RETRY == type) || (WIFI_WAIT == type)) { return; }
#if defined(USE_WEBSERVER) && defined(USE_EMULATION)
    UdpDisconnect();
#endif  // USE_EMULATION
    WiFi.disconnect();                       // Solve possible Wifi hangs
    wifi_config_type = type;

#ifndef USE_WPS
    if (WIFI_WPSCONFIG == wifi_config_type) { wifi_config_type = WIFI_MANAGER; }
#endif  // USE_WPS
#ifndef USE_WEBSERVER
    if (WIFI_MANAGER == wifi_config_type) { wifi_config_type = WIFI_SMARTCONFIG; }
#endif  // USE_WEBSERVER
#ifndef USE_SMARTCONFIG
    if (WIFI_SMARTCONFIG == wifi_config_type) { wifi_config_type = WIFI_SERIAL; }
#endif  // USE_SMARTCONFIG

    wifi_config_counter = WIFI_CONFIG_SEC;   // Allow up to WIFI_CONFIG_SECS seconds for phone to provide ssid/pswd
    wifi_counter = wifi_config_counter +5;
    blinks = 1999;
    if (WIFI_RESTART == wifi_config_type) {
      restart_flag = 2;
    }
    else if (WIFI_SERIAL == wifi_config_type) {
      AddLog_P(LOG_LEVEL_INFO, S_LOG_WIFI, PSTR(D_WCFG_6_SERIAL " " D_ACTIVE_FOR_3_MINUTES));
    }
#ifdef USE_SMARTCONFIG
    else if (WIFI_SMARTCONFIG == wifi_config_type) {
      AddLog_P(LOG_LEVEL_INFO, S_LOG_WIFI, PSTR(D_WCFG_1_SMARTCONFIG " " D_ACTIVE_FOR_3_MINUTES));
      WiFi.beginSmartConfig();
    }
#endif  // USE_SMARTCONFIG
#ifdef USE_WPS
    else if (WIFI_WPSCONFIG == wifi_config_type) {
      if (WifiWpsConfigBegin()) {
        AddLog_P(LOG_LEVEL_INFO, S_LOG_WIFI, PSTR(D_WCFG_3_WPSCONFIG " " D_ACTIVE_FOR_3_MINUTES));
      } else {
        AddLog_P(LOG_LEVEL_INFO, S_LOG_WIFI, PSTR(D_WCFG_3_WPSCONFIG " " D_FAILED_TO_START));
        wifi_config_counter = 3;
      }
    }
#endif  // USE_WPS
#ifdef USE_WEBSERVER
    else if (WIFI_MANAGER == wifi_config_type) {
      AddLog_P(LOG_LEVEL_INFO, S_LOG_WIFI, PSTR(D_WCFG_2_WIFIMANAGER " " D_ACTIVE_FOR_3_MINUTES));
      WifiManagerBegin();
    }
#endif  // USE_WEBSERVER
  }
}

void WiFiSetSleepMode()
{
/* Excerpt from the esp8266 non os sdk api reference (v2.2.1):
 * Sets sleep type for power saving. Set WIFI_NONE_SLEEP to disable power saving.
 * - Default mode: WIFI_MODEM_SLEEP.
 * - In order to lower the power comsumption, ESP8266 changes the TCP timer
 *   tick from 250ms to 3s in WIFI_LIGHT_SLEEP mode, which leads to increased timeout for
 *   TCP timer. Therefore, the WIFI_MODEM_SLEEP or deep-sleep mode should be used
 *   where there is a requirement for the accurancy of the TCP timer.
 *
 * Sleep is disabled in core 2.4.1 and 2.4.2 as there are bugs in their SDKs
 * See https://github.com/arendst/Sonoff-Tasmota/issues/2559
 */

// Sleep explanation: https://github.com/esp8266/Arduino/blob/3f0c601cfe81439ce17e9bd5d28994a7ed144482/libraries/ESP8266WiFi/src/ESP8266WiFiGeneric.cpp#L255
#if defined(ARDUINO_ESP8266_RELEASE_2_4_1) || defined(ARDUINO_ESP8266_RELEASE_2_4_2)
#else  // Enabled in 2.3.0, 2.4.0 and stage
  if (sleep) {
    WiFi.setSleepMode(WIFI_LIGHT_SLEEP);  // Allow light sleep during idle times
  } else {
    WiFi.setSleepMode(WIFI_MODEM_SLEEP);  // Disable sleep (Esp8288/Arduino core and sdk default)
  }
#endif
}

void WifiBegin(uint8_t flag)
{
  const char kWifiPhyMode[] = " BGN";

#if defined(USE_WEBSERVER) && defined(USE_EMULATION)
  UdpDisconnect();
#endif  // USE_EMULATION

#ifdef ARDUINO_ESP8266_RELEASE_2_3_0  // (!strncmp_P(ESP.getSdkVersion(),PSTR("1.5.3"),5))
  AddLog_P(LOG_LEVEL_DEBUG, S_LOG_WIFI, PSTR(D_PATCH_ISSUE_2186));
  WiFi.mode(WIFI_OFF);      // See https://github.com/esp8266/Arduino/issues/2186
#endif

  WiFi.persistent(false);   // Solve possible wifi init errors (re-add at 6.2.1.16 #4044, #4083)
  WiFi.disconnect(true);    // Delete SDK wifi config
  delay(200);
  WiFi.mode(WIFI_STA);      // Disable AP mode
  WiFiSetSleepMode();
//  if (WiFi.getPhyMode() != WIFI_PHY_MODE_11N) { WiFi.setPhyMode(WIFI_PHY_MODE_11N); }
  if (!WiFi.getAutoConnect()) { WiFi.setAutoConnect(true); }
//  WiFi.setAutoReconnect(true);
  switch (flag) {
  case 0:  // AP1
  case 1:  // AP2
    Settings.sta_active = flag;
    break;
  case 2:  // Toggle
    Settings.sta_active ^= 1;
  }        // 3: Current AP
  if ('\0' == Settings.sta_ssid[Settings.sta_active][0]) { Settings.sta_active ^= 1; }  // Skip empty SSID
  if (Settings.ip_address[0]) {
    WiFi.config(Settings.ip_address[0], Settings.ip_address[1], Settings.ip_address[2], Settings.ip_address[3]);  // Set static IP
  }
  WiFi.hostname(my_hostname);
  WiFi.begin(Settings.sta_ssid[Settings.sta_active], Settings.sta_pwd[Settings.sta_active]);
  snprintf_P(log_data, sizeof(log_data), PSTR(D_LOG_WIFI D_CONNECTING_TO_AP "%d %s " D_IN_MODE " 11%c " D_AS " %s..."),
    Settings.sta_active +1, Settings.sta_ssid[Settings.sta_active], kWifiPhyMode[WiFi.getPhyMode() & 0x3], my_hostname);
  AddLog(LOG_LEVEL_INFO);
}

void WifiSetState(uint8_t state)
{
  if (state == global_state.wifi_down) {
    if (state) {
      rules_flag.wifi_connected = 1;
    } else {
      rules_flag.wifi_disconnected = 1;
    }
  }
  global_state.wifi_down = state ^1;
}

void WifiCheckIp()
{
  if ((WL_CONNECTED == WiFi.status()) && (static_cast<uint32_t>(WiFi.localIP()) != 0)) {
    WifiSetState(1);
    wifi_counter = WIFI_CHECK_SEC;
    wifi_retry = wifi_retry_init;
    AddLog_P((wifi_status != WL_CONNECTED) ? LOG_LEVEL_INFO : LOG_LEVEL_DEBUG_MORE, S_LOG_WIFI, PSTR(D_CONNECTED));
    if (wifi_status != WL_CONNECTED) {
//      AddLog_P(LOG_LEVEL_INFO, PSTR("Wifi: Set IP addresses"));
      Settings.ip_address[1] = (uint32_t)WiFi.gatewayIP();
      Settings.ip_address[2] = (uint32_t)WiFi.subnetMask();
      Settings.ip_address[3] = (uint32_t)WiFi.dnsIP();
    }
    wifi_status = WL_CONNECTED;
  } else {
    WifiSetState(0);
    uint8_t wifi_config_tool = Settings.sta_config;
    wifi_status = WiFi.status();
    switch (wifi_status) {
      case WL_CONNECTED:
        AddLog_P(LOG_LEVEL_INFO, S_LOG_WIFI, PSTR(D_CONNECT_FAILED_NO_IP_ADDRESS));
        wifi_status = 0;
        wifi_retry = wifi_retry_init;
        break;
      case WL_NO_SSID_AVAIL:
        AddLog_P(LOG_LEVEL_INFO, S_LOG_WIFI, PSTR(D_CONNECT_FAILED_AP_NOT_REACHED));
        if (WIFI_WAIT == Settings.sta_config) {
          wifi_retry = wifi_retry_init;
        } else {
          if (wifi_retry > (wifi_retry_init / 2)) {
            wifi_retry = wifi_retry_init / 2;
          }
          else if (wifi_retry) {
            wifi_retry = 0;
          }
        }
        break;
      case WL_CONNECT_FAILED:
        AddLog_P(LOG_LEVEL_INFO, S_LOG_WIFI, PSTR(D_CONNECT_FAILED_WRONG_PASSWORD));
        if (wifi_retry > (wifi_retry_init / 2)) {
          wifi_retry = wifi_retry_init / 2;
        }
        else if (wifi_retry) {
          wifi_retry = 0;
        }
        break;
      default:  // WL_IDLE_STATUS and WL_DISCONNECTED
        if (!wifi_retry || ((wifi_retry_init / 2) == wifi_retry)) {
          AddLog_P(LOG_LEVEL_INFO, S_LOG_WIFI, PSTR(D_CONNECT_FAILED_AP_TIMEOUT));
        } else {
          if (('\0' == Settings.sta_ssid[0][0]) && ('\0' == Settings.sta_ssid[1][0])) {
            wifi_config_tool = WIFI_CONFIG_NO_SSID;    // Skip empty SSIDs and start Wifi config tool
            wifi_retry = 0;
          } else {
            AddLog_P(LOG_LEVEL_DEBUG, S_LOG_WIFI, PSTR(D_ATTEMPTING_CONNECTION));
          }
        }
    }
    if (wifi_retry) {
      if (wifi_retry_init == wifi_retry) {
        WifiBegin(3);  // Select default SSID
      }
      if ((Settings.sta_config != WIFI_WAIT) && ((wifi_retry_init / 2) == wifi_retry)) {
        WifiBegin(2);  // Select alternate SSID
      }
      wifi_counter = 1;
      wifi_retry--;
    } else {
      WifiConfig(wifi_config_tool);
      wifi_counter = 1;
      wifi_retry = wifi_retry_init;
    }
  }
}

void WifiCheck(uint8_t param)
{
  wifi_counter--;
  switch (param) {
  case WIFI_SERIAL:
  case WIFI_SMARTCONFIG:
  case WIFI_MANAGER:
  case WIFI_WPSCONFIG:
    WifiConfig(param);
    break;
  default:
    if (wifi_config_counter) {
      wifi_config_counter--;
      wifi_counter = wifi_config_counter +5;
      if (wifi_config_counter) {
#ifdef USE_SMARTCONFIG
        if ((WIFI_SMARTCONFIG == wifi_config_type) && WiFi.smartConfigDone()) {
          wifi_config_counter = 0;
        }
#endif  // USE_SMARTCONFIG
#ifdef USE_WPS
        if ((WIFI_WPSCONFIG == wifi_config_type) && WifiWpsConfigDone()) {
          wifi_config_counter = 0;
        }
#endif  // USE_WPS
        if (!wifi_config_counter) {
          if (strlen(WiFi.SSID().c_str())) {
            strlcpy(Settings.sta_ssid[0], WiFi.SSID().c_str(), sizeof(Settings.sta_ssid[0]));
          }
          if (strlen(WiFi.psk().c_str())) {
            strlcpy(Settings.sta_pwd[0], WiFi.psk().c_str(), sizeof(Settings.sta_pwd[0]));
          }
          Settings.sta_active = 0;
          snprintf_P(log_data, sizeof(log_data), PSTR(D_LOG_WIFI D_WCFG_1_SMARTCONFIG D_CMND_SSID "1 %s"), Settings.sta_ssid[0]);
          AddLog(LOG_LEVEL_INFO);
        }
      }
      if (!wifi_config_counter) {
#ifdef USE_SMARTCONFIG
        if (WIFI_SMARTCONFIG == wifi_config_type) { WiFi.stopSmartConfig(); }
#endif  // USE_SMARTCONFIG
//        SettingsSdkErase();  //  Disabled v6.1.0b due to possible bad wifi connects
        restart_flag = 2;
      }
    } else {
      if (wifi_counter <= 0) {
        AddLog_P(LOG_LEVEL_DEBUG_MORE, S_LOG_WIFI, PSTR(D_CHECKING_CONNECTION));
        wifi_counter = WIFI_CHECK_SEC;
        WifiCheckIp();
      }
      if ((WL_CONNECTED == WiFi.status()) && (static_cast<uint32_t>(WiFi.localIP()) != 0) && !wifi_config_type) {
        WifiSetState(1);
#ifdef BE_MINIMAL
        if (1 == RtcSettings.ota_loader) {
          RtcSettings.ota_loader = 0;
          ota_state_flag = 3;
        }
#endif  // BE_MINIMAL

#ifdef USE_DISCOVERY
        if (!mdns_begun) {
          if (mdns_delayed_start) {
            AddLog_P(LOG_LEVEL_INFO, PSTR(D_LOG_MDNS D_ATTEMPTING_CONNECTION));
            mdns_delayed_start--;
          } else {
            mdns_delayed_start = Settings.param[P_MDNS_DELAYED_START];
            mdns_begun = MDNS.begin(my_hostname);
            snprintf_P(log_data, sizeof(log_data), PSTR(D_LOG_MDNS "%s"), (mdns_begun) ? D_INITIALIZED : D_FAILED);
            AddLog(LOG_LEVEL_INFO);
          }
        }
#endif  // USE_DISCOVERY

#ifdef USE_WEBSERVER
        if (Settings.webserver) {
          StartWebserver(Settings.webserver, WiFi.localIP());
#ifdef USE_DISCOVERY
#ifdef WEBSERVER_ADVERTISE
          if (mdns_begun) {
            MDNS.addService("http", "tcp", WEB_PORT);
          }
#endif  // WEBSERVER_ADVERTISE
#endif  // USE_DISCOVERY
        } else {
          StopWebserver();
        }
#ifdef USE_EMULATION
        if (Settings.flag2.emulation) { UdpConnect(); }
#endif  // USE_EMULATION
#endif  // USE_WEBSERVER

#ifdef USE_KNX
        if (!knx_started && Settings.flag.knx_enabled) {
          KNXStart();
          knx_started = true;
        }
#endif  // USE_KNX

      } else {
        WifiSetState(0);
#if defined(USE_WEBSERVER) && defined(USE_EMULATION)
        UdpDisconnect();
#endif  // USE_EMULATION
        mdns_begun = false;
#ifdef USE_KNX
        knx_started = false;
#endif  // USE_KNX
      }
    }
  }
}

int WifiState()
{
  int state = -1;

  if (!global_state.wifi_down) { state = WIFI_RESTART; }
  if (wifi_config_type) { state = wifi_config_type; }
  return state;
}

void WifiConnect()
{
  WifiSetState(0);
  WiFi.persistent(false);    // Solve possible wifi init errors
  wifi_status = 0;
  wifi_retry_init = WIFI_RETRY_OFFSET_SEC + ((ESP.getChipId() & 0xF) * 2);
  wifi_retry = wifi_retry_init;
  wifi_counter = 1;
}

/*
// Enable from 6.0.0a until 6.1.0a - disabled due to possible cause of bad wifi connect on core 2.3.0
void WifiDisconnect()
{
  // Courtesy of EspEasy
  WiFi.persistent(true);      // use SDK storage of SSID/WPA parameters
  ETS_UART_INTR_DISABLE();
  wifi_station_disconnect();  // this will store empty ssid/wpa into sdk storage
  ETS_UART_INTR_ENABLE();
  WiFi.persistent(false);     // Do not use SDK storage of SSID/WPA parameters
}

void EspRestart()
{
  // This results in exception 3 on restarts
  delay(100);                 // Allow time for message xfer - disabled v6.1.0b
  WifiDisconnect();
  ESP.restart();
}
*/

void EspRestart()
{
  ESP.restart();
}

/*********************************************************************************************\
 * Basic I2C routines
\*********************************************************************************************/

#ifdef USE_I2C
#define I2C_RETRY_COUNTER 3

uint32_t i2c_buffer = 0;

bool I2cValidRead(uint8_t addr, uint8_t reg, uint8_t size)
{
  byte x = I2C_RETRY_COUNTER;

  i2c_buffer = 0;
  do {
    Wire.beginTransmission(addr);                       // start transmission to device
    Wire.write(reg);                                    // sends register address to read from
    if (0 == Wire.endTransmission(false)) {             // Try to become I2C Master, send data and collect bytes, keep master status for next request...
      Wire.requestFrom((int)addr, (int)size);           // send data n-bytes read
      if (Wire.available() == size) {
        for (byte i = 0; i < size; i++) {
          i2c_buffer = i2c_buffer << 8 | Wire.read();   // receive DATA
        }
      }
    }
    x--;
  } while (Wire.endTransmission(true) != 0 && x != 0);  // end transmission
  return (x);
}

bool I2cValidRead8(uint8_t *data, uint8_t addr, uint8_t reg)
{
  bool status = I2cValidRead(addr, reg, 1);
  *data = (uint8_t)i2c_buffer;
  return status;
}

bool I2cValidRead16(uint16_t *data, uint8_t addr, uint8_t reg)
{
  bool status = I2cValidRead(addr, reg, 2);
  *data = (uint16_t)i2c_buffer;
  return status;
}

bool I2cValidReadS16(int16_t *data, uint8_t addr, uint8_t reg)
{
  bool status = I2cValidRead(addr, reg, 2);
  *data = (int16_t)i2c_buffer;
  return status;
}

bool I2cValidRead16LE(uint16_t *data, uint8_t addr, uint8_t reg)
{
  uint16_t ldata;
  bool status = I2cValidRead16(&ldata, addr, reg);
  *data = (ldata >> 8) | (ldata << 8);
  return status;
}

bool I2cValidReadS16_LE(int16_t *data, uint8_t addr, uint8_t reg)
{
  uint16_t ldata;
  bool status = I2cValidRead16LE(&ldata, addr, reg);
  *data = (int16_t)ldata;
  return status;
}

bool I2cValidRead24(int32_t *data, uint8_t addr, uint8_t reg)
{
  bool status = I2cValidRead(addr, reg, 3);
  *data = i2c_buffer;
  return status;
}

uint8_t I2cRead8(uint8_t addr, uint8_t reg)
{
  I2cValidRead(addr, reg, 1);
  return (uint8_t)i2c_buffer;
}

uint16_t I2cRead16(uint8_t addr, uint8_t reg)
{
  I2cValidRead(addr, reg, 2);
  return (uint16_t)i2c_buffer;
}

int16_t I2cReadS16(uint8_t addr, uint8_t reg)
{
  I2cValidRead(addr, reg, 2);
  return (int16_t)i2c_buffer;
}

uint16_t I2cRead16LE(uint8_t addr, uint8_t reg)
{
  I2cValidRead(addr, reg, 2);
  uint16_t temp = (uint16_t)i2c_buffer;
  return (temp >> 8) | (temp << 8);
}

int16_t I2cReadS16_LE(uint8_t addr, uint8_t reg)
{
  return (int16_t)I2cRead16LE(addr, reg);
}

int32_t I2cRead24(uint8_t addr, uint8_t reg)
{
  I2cValidRead(addr, reg, 3);
  return i2c_buffer;
}

bool I2cWrite(uint8_t addr, uint8_t reg, uint32_t val, uint8_t size)
{
  byte x = I2C_RETRY_COUNTER;

  do {
    Wire.beginTransmission((uint8_t)addr);              // start transmission to device
    Wire.write(reg);                                    // sends register address to write to
    uint8_t bytes = size;
    while (bytes--) {
      Wire.write((val >> (8 * bytes)) & 0xFF);          // write data
    }
    x--;
  } while (Wire.endTransmission(true) != 0 && x != 0);  // end transmission
  return (x);
}

bool I2cWrite8(uint8_t addr, uint8_t reg, uint16_t val)
{
   return I2cWrite(addr, reg, val, 1);
}

bool I2cWrite16(uint8_t addr, uint8_t reg, uint16_t val)
{
   return I2cWrite(addr, reg, val, 2);
}

int8_t I2cReadBuffer(uint8_t addr, uint8_t reg, uint8_t *reg_data, uint16_t len)
{
  Wire.beginTransmission((uint8_t)addr);
  Wire.write((uint8_t)reg);
  Wire.endTransmission();
  if (len != Wire.requestFrom((uint8_t)addr, (byte)len)) {
    return 1;
  }
  while (len--) {
    *reg_data = (uint8_t)Wire.read();
    reg_data++;
  }
  return 0;
}

int8_t I2cWriteBuffer(uint8_t addr, uint8_t reg, uint8_t *reg_data, uint16_t len)
{
  Wire.beginTransmission((uint8_t)addr);
  Wire.write((uint8_t)reg);
  while (len--) {
    Wire.write(*reg_data);
    reg_data++;
  }
  Wire.endTransmission();
  return 0;
}

void I2cScan(char *devs, unsigned int devs_len)
{
  // Return error codes defined in twi.h and core_esp8266_si2c.c
  // I2C_OK                      0
  // I2C_SCL_HELD_LOW            1 = SCL held low by another device, no procedure available to recover
  // I2C_SCL_HELD_LOW_AFTER_READ 2 = I2C bus error. SCL held low beyond slave clock stretch time
  // I2C_SDA_HELD_LOW            3 = I2C bus error. SDA line held low by slave/another_master after n bits
  // I2C_SDA_HELD_LOW_AFTER_INIT 4 = line busy. SDA again held low by another device. 2nd master?

  byte error = 0;
  byte address = 0;
  byte any = 0;

  snprintf_P(devs, devs_len, PSTR("{\"" D_CMND_I2CSCAN "\":\"" D_JSON_I2CSCAN_DEVICES_FOUND_AT));
  for (address = 1; address <= 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    if (0 == error) {
      any = 1;
      snprintf_P(devs, devs_len, PSTR("%s 0x%02x"), devs, address);
    }
    else if (error != 2) {  // Seems to happen anyway using this scan
      any = 2;
      snprintf_P(devs, devs_len, PSTR("{\"" D_CMND_I2CSCAN "\":\"Error %d at 0x%02x"), error, address);
      break;
    }
  }
  if (any) {
    strncat(devs, "\"}", devs_len);
  }
  else {
    snprintf_P(devs, devs_len, PSTR("{\"" D_CMND_I2CSCAN "\":\"" D_JSON_I2CSCAN_NO_DEVICES_FOUND "\"}"));
  }
}

boolean I2cDevice(byte addr)
{
  for (byte address = 1; address <= 127; address++) {
    Wire.beginTransmission(address);
    if (!Wire.endTransmission() && (address == addr)) {
      return true;
    }
  }
  return false;
}
#endif  // USE_I2C

/*********************************************************************************************\
 * Real Time Clock
 *
 * Sources: Time by Michael Margolis and Paul Stoffregen (https://github.com/PaulStoffregen/Time)
 *          Timezone by Jack Christensen (https://github.com/JChristensen/Timezone)
\*********************************************************************************************/

extern "C" {
#include "sntp.h"
}

#define SECS_PER_MIN  ((uint32_t)(60UL))
#define SECS_PER_HOUR ((uint32_t)(3600UL))
#define SECS_PER_DAY  ((uint32_t)(SECS_PER_HOUR * 24UL))
#define MINS_PER_HOUR ((uint32_t)(60UL))
#define LEAP_YEAR(Y)  (((1970+Y)>0) && !((1970+Y)%4) && (((1970+Y)%100) || !((1970+Y)%400)))

Ticker TickerRtc;

static const uint8_t kDaysInMonth[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }; // API starts months from 1, this array starts from 0
static const char kMonthNamesEnglish[] = "JanFebMarAprMayJunJulAugSepOctNovDec";

uint32_t utc_time = 0;
uint32_t local_time = 0;
uint32_t daylight_saving_time = 0;
uint32_t standard_time = 0;
uint32_t ntp_time = 0;
uint32_t midnight = 1451602800;
uint32_t restart_time = 0;
int32_t  time_timezone = 0;
uint8_t  midnight_now = 0;
uint8_t  ntp_sync_minute = 0;

String GetBuildDateAndTime()
{
  // "2017-03-07T11:08:02" - ISO8601:2004
  char bdt[21];
  char *p;
  char mdate[] = __DATE__;  // "Mar  7 2017"
  char *smonth = mdate;
  int day = 0;
  int year = 0;

  // sscanf(mdate, "%s %d %d", bdt, &day, &year);  // Not implemented in 2.3.0 and probably too much code
  byte i = 0;
  for (char *str = strtok_r(mdate, " ", &p); str && i < 3; str = strtok_r(NULL, " ", &p)) {
    switch (i++) {
    case 0:  // Month
      smonth = str;
      break;
    case 1:  // Day
      day = atoi(str);
      break;
    case 2:  // Year
      year = atoi(str);
    }
  }
  int month = (strstr(kMonthNamesEnglish, smonth) -kMonthNamesEnglish) /3 +1;
  snprintf_P(bdt, sizeof(bdt), PSTR("%d" D_YEAR_MONTH_SEPARATOR "%02d" D_MONTH_DAY_SEPARATOR "%02d" D_DATE_TIME_SEPARATOR "%s"), year, month, day, __TIME__);
  return String(bdt);  // 2017-03-07T11:08:02
}

String GetTimeZone()
{
  char tz[7];

  snprintf_P(tz, sizeof(tz), PSTR("%+03d:%02d"), time_timezone / 60, abs(time_timezone % 60));

  return String(tz);  // -03:45
}

/*
 * timestamps in https://en.wikipedia.org/wiki/ISO_8601 format
 *
 *  DT_UTC - current data and time in Greenwich, England (aka GMT)
 *  DT_LOCAL - current date and time taking timezone into account
 *  DT_RESTART - the date and time this device last started, in local timezone
 *
 * Format:
 *  "2017-03-07T11:08:02-07:00" - if DT_LOCAL and SetOption52 = 1
 *  "2017-03-07T11:08:02"       - otherwise
 */
String GetDateAndTime(byte time_type)
{
  // "2017-03-07T11:08:02-07:00" - ISO8601:2004
  char dt[27];
  TIME_T tmpTime;

  switch (time_type) {
    case DT_ENERGY:
      BreakTime(Settings.energy_kWhtotal_time, tmpTime);
      tmpTime.year += 1970;
      break;
    case DT_UTC:
      BreakTime(utc_time, tmpTime);
      tmpTime.year += 1970;
      break;
    case DT_RESTART:
      if (restart_time == 0) {
        return "";
      }
      BreakTime(restart_time, tmpTime);
      tmpTime.year += 1970;
      break;
    default:
      tmpTime = RtcTime;
  }

  snprintf_P(dt, sizeof(dt), PSTR("%04d-%02d-%02dT%02d:%02d:%02d"),
    tmpTime.year, tmpTime.month, tmpTime.day_of_month, tmpTime.hour, tmpTime.minute, tmpTime.second);

  if (Settings.flag3.time_append_timezone && (DT_LOCAL == time_type)) {
//  if (Settings.flag3.time_append_timezone && ((DT_LOCAL == time_type) || (DT_ENERGY == time_type))) {
    strncat(dt, GetTimeZone().c_str(), sizeof(dt));
  }

  return String(dt);  // 2017-03-07T11:08:02-07:00
}

String GetTime(int type)
{
  /* type 1 - Local time
   * type 2 - Daylight Savings time
   * type 3 - Standard time
   */
  char stime[25];   // Skip newline

  uint32_t time = utc_time;
  if (1 == type) time = local_time;
  if (2 == type) time = daylight_saving_time;
  if (3 == type) time = standard_time;
  snprintf_P(stime, sizeof(stime), sntp_get_real_time(time));

  return String(stime);  // Thu Nov 01 11:41:02 2018
}

String GetUptime()
{
  char dt[16];

  TIME_T ut;

  if (restart_time) {
    BreakTime(utc_time - restart_time, ut);
  } else {
    BreakTime(uptime, ut);
  }

  // "P128DT14H35M44S" - ISO8601:2004 - https://en.wikipedia.org/wiki/ISO_8601 Durations
//  snprintf_P(dt, sizeof(dt), PSTR("P%dDT%02dH%02dM%02dS"), ut.days, ut.hour, ut.minute, ut.second);

  // "128 14:35:44" - OpenVMS
  // "128T14:35:44" - Tasmota
  snprintf_P(dt, sizeof(dt), PSTR("%dT%02d:%02d:%02d"), ut.days, ut.hour, ut.minute, ut.second);

  return String(dt);  // 128T14:35:44
}

uint32_t GetMinutesUptime()
{
  TIME_T ut;

  if (restart_time) {
    BreakTime(utc_time - restart_time, ut);
  } else {
    BreakTime(uptime, ut);
  }

  return (ut.days *1440) + (ut.hour *60) + ut.minute;
}

uint32_t GetMinutesPastMidnight()
{
  uint32_t minutes = 0;

  if (RtcTime.valid) {
    minutes = (RtcTime.hour *60) + RtcTime.minute;
  }
  return minutes;
}

void BreakTime(uint32_t time_input, TIME_T &tm)
{
// break the given time_input into time components
// this is a more compact version of the C library localtime function
// note that year is offset from 1970 !!!

  uint8_t year;
  uint8_t month;
  uint8_t month_length;
  uint32_t time;
  unsigned long days;

  time = time_input;
  tm.second = time % 60;
  time /= 60;                // now it is minutes
  tm.minute = time % 60;
  time /= 60;                // now it is hours
  tm.hour = time % 24;
  time /= 24;                // now it is days
  tm.days = time;
  tm.day_of_week = ((time + 4) % 7) + 1;  // Sunday is day 1

  year = 0;
  days = 0;
  while((unsigned)(days += (LEAP_YEAR(year) ? 366 : 365)) <= time) {
    year++;
  }
  tm.year = year;            // year is offset from 1970

  days -= LEAP_YEAR(year) ? 366 : 365;
  time -= days;              // now it is days in this year, starting at 0
  tm.day_of_year = time;

  days = 0;
  month = 0;
  month_length = 0;
  for (month = 0; month < 12; month++) {
    if (1 == month) { // february
      if (LEAP_YEAR(year)) {
        month_length = 29;
      } else {
        month_length = 28;
      }
    } else {
      month_length = kDaysInMonth[month];
    }

    if (time >= month_length) {
      time -= month_length;
    } else {
      break;
    }
  }
  strlcpy(tm.name_of_month, kMonthNames + (month *3), 4);
  tm.month = month + 1;      // jan is month 1
  tm.day_of_month = time + 1;         // day of month
  tm.valid = (time_input > 1451602800);  // 2016-01-01
}

uint32_t MakeTime(TIME_T &tm)
{
// assemble time elements into time_t
// note year argument is offset from 1970

  int i;
  uint32_t seconds;

  // seconds from 1970 till 1 jan 00:00:00 of the given year
  seconds = tm.year * (SECS_PER_DAY * 365);
  for (i = 0; i < tm.year; i++) {
    if (LEAP_YEAR(i)) {
      seconds +=  SECS_PER_DAY;   // add extra days for leap years
    }
  }

  // add days for this year, months start from 1
  for (i = 1; i < tm.month; i++) {
    if ((2 == i) && LEAP_YEAR(tm.year)) {
      seconds += SECS_PER_DAY * 29;
    } else {
      seconds += SECS_PER_DAY * kDaysInMonth[i-1];  // monthDay array starts from 0
    }
  }
  seconds+= (tm.day_of_month - 1) * SECS_PER_DAY;
  seconds+= tm.hour * SECS_PER_HOUR;
  seconds+= tm.minute * SECS_PER_MIN;
  seconds+= tm.second;
  return seconds;
}

uint32_t RuleToTime(TimeRule r, int yr)
{
  TIME_T tm;
  uint32_t t;
  uint8_t m;
  uint8_t w;                // temp copies of r.month and r.week

  m = r.month;
  w = r.week;
  if (0 == w) {             // Last week = 0
    if (++m > 12) {         // for "Last", go to the next month
      m = 1;
      yr++;
    }
    w = 1;                  // and treat as first week of next month, subtract 7 days later
  }

  tm.hour = r.hour;
  tm.minute = 0;
  tm.second = 0;
  tm.day_of_month = 1;
  tm.month = m;
  tm.year = yr - 1970;
  t = MakeTime(tm);         // First day of the month, or first day of next month for "Last" rules
  BreakTime(t, tm);
  t += (7 * (w - 1) + (r.dow - tm.day_of_week + 7) % 7) * SECS_PER_DAY;
  if (0 == r.week) {
    t -= 7 * SECS_PER_DAY;  // back up a week if this is a "Last" rule
  }
  return t;
}

uint32_t LocalTime()
{
  return local_time;
}

uint32_t Midnight()
{
  return midnight;
}

boolean MidnightNow()
{
  boolean mnflg = midnight_now;
  if (mnflg) midnight_now = 0;
  return mnflg;
}

void RtcSecond()
{
  TIME_T tmpTime;

  if ((ntp_sync_minute > 59) && (RtcTime.minute > 2)) ntp_sync_minute = 1;                 // If sync prepare for a new cycle
  uint8_t offset = (uptime < 30) ? RtcTime.second : (((ESP.getChipId() & 0xF) * 3) + 3) ;  // First try ASAP to sync. If fails try once every 60 seconds based on chip id
  if (!global_state.wifi_down && (offset == RtcTime.second) && ((RtcTime.year < 2016) || (ntp_sync_minute == RtcTime.minute) || ntp_force_sync)) {
    ntp_time = sntp_get_current_timestamp();
    if (ntp_time > 1451602800) {  // Fix NTP bug in core 2.4.1/SDK 2.2.1 (returns Thu Jan 01 08:00:10 1970 after power on)
      ntp_force_sync = 0;
      utc_time = ntp_time;
      ntp_sync_minute = 60;  // Sync so block further requests
      if (restart_time == 0) {
        restart_time = utc_time - uptime;  // save first ntp time as restart time
      }
      BreakTime(utc_time, tmpTime);
      RtcTime.year = tmpTime.year + 1970;
      daylight_saving_time = RuleToTime(Settings.tflag[1], RtcTime.year);
      standard_time = RuleToTime(Settings.tflag[0], RtcTime.year);
      snprintf_P(log_data, sizeof(log_data), PSTR(D_LOG_APPLICATION "(" D_UTC_TIME ") %s, (" D_DST_TIME ") %s, (" D_STD_TIME ") %s"),
        GetTime(0).c_str(), GetTime(2).c_str(), GetTime(3).c_str());
      AddLog(LOG_LEVEL_DEBUG);
      if (local_time < 1451602800) {  // 2016-01-01
        rules_flag.time_init = 1;
      } else {
        rules_flag.time_set = 1;
      }
    } else {
      ntp_sync_minute++;  // Try again in next minute
    }
  }
  utc_time++;
  local_time = utc_time;
  if (local_time > 1451602800) {  // 2016-01-01
    int16_t timezone_minutes = Settings.timezone_minutes;
    if (Settings.timezone < 0) { timezone_minutes *= -1; }
    time_timezone = (Settings.timezone * SECS_PER_HOUR) + (timezone_minutes * SECS_PER_MIN);
    if (99 == Settings.timezone) {
      int32_t dstoffset = Settings.toffset[1] * SECS_PER_MIN;
      int32_t stdoffset = Settings.toffset[0] * SECS_PER_MIN;
      if (Settings.tflag[1].hemis) {
        // Southern hemisphere
        if ((utc_time >= (standard_time - dstoffset)) && (utc_time < (daylight_saving_time - stdoffset))) {
          time_timezone = stdoffset;  // Standard Time
        } else {
          time_timezone = dstoffset;  // Daylight Saving Time
        }
      } else {
        // Northern hemisphere
        if ((utc_time >= (daylight_saving_time - stdoffset)) && (utc_time < (standard_time - dstoffset))) {
          time_timezone = dstoffset;  // Daylight Saving Time
        } else {
          time_timezone = stdoffset;  // Standard Time
        }
      }
    }
    local_time += time_timezone;
    time_timezone /= 60;
    if (!Settings.energy_kWhtotal_time) { Settings.energy_kWhtotal_time = local_time; }
  }
  BreakTime(local_time, RtcTime);
  if (!RtcTime.hour && !RtcTime.minute && !RtcTime.second && RtcTime.valid) {
    midnight = local_time;
    midnight_now = 1;
  }
  RtcTime.year += 1970;
}

void RtcInit()
{
  sntp_setservername(0, Settings.ntp_server[0]);
  sntp_setservername(1, Settings.ntp_server[1]);
  sntp_setservername(2, Settings.ntp_server[2]);
  sntp_stop();
  sntp_set_timezone(0);      // UTC time
  sntp_init();
  utc_time = 0;
  BreakTime(utc_time, RtcTime);
  TickerRtc.attach(1, RtcSecond);
}

/*********************************************************************************************\
 * Syslog
 *
 * Example:
 *   snprintf_P(log_data, sizeof(log_data), PSTR(D_LOG_LOG "Any value %d"), value);
 *   AddLog(LOG_LEVEL_DEBUG);
 *
\*********************************************************************************************/

void SetSeriallog(byte loglevel)
{
  Settings.seriallog_level = loglevel;
  seriallog_level = loglevel;
  seriallog_timer = 0;
}

#ifdef USE_WEBSERVER
void GetLog(byte idx, char** entry_pp, size_t* len_p)
{
  char* entry_p = NULL;
  size_t len = 0;

  if (idx) {
    char* it = web_log;
    do {
      byte cur_idx = *it;
      it++;
      size_t tmp = strchrspn(it, '\1');
      tmp++;                             // Skip terminating '\1'
      if (cur_idx == idx) {              // Found the requested entry
        len = tmp;
        entry_p = it;
        break;
      }
      it += tmp;
    } while (it < web_log + WEB_LOG_SIZE && *it != '\0');
  }
  *entry_pp = entry_p;
  *len_p = len;
}
#endif  // USE_WEBSERVER

void Syslog()
{
  // Destroys log_data
  char syslog_preamble[64];  // Hostname + Id

  if (syslog_host_hash != GetHash(Settings.syslog_host, strlen(Settings.syslog_host))) {
    syslog_host_hash = GetHash(Settings.syslog_host, strlen(Settings.syslog_host));
    WiFi.hostByName(Settings.syslog_host, syslog_host_addr);  // If sleep enabled this might result in exception so try to do it once using hash
  }
  if (PortUdp.beginPacket(syslog_host_addr, Settings.syslog_port)) {
    snprintf_P(syslog_preamble, sizeof(syslog_preamble), PSTR("%s ESP-"), my_hostname);
    memmove(log_data + strlen(syslog_preamble), log_data, sizeof(log_data) - strlen(syslog_preamble));
    log_data[sizeof(log_data) -1] = '\0';
    memcpy(log_data, syslog_preamble, strlen(syslog_preamble));
    PortUdp.write(log_data);
    PortUdp.endPacket();
  } else {
    syslog_level = 0;
    syslog_timer = SYSLOG_TIMER;
    snprintf_P(log_data, sizeof(log_data), PSTR(D_LOG_APPLICATION D_SYSLOG_HOST_NOT_FOUND ". " D_RETRY_IN " %d " D_UNIT_SECOND), SYSLOG_TIMER);
    AddLog(LOG_LEVEL_INFO);
  }
}

void AddLog(byte loglevel)
{
  char mxtime[10];  // "13:45:21 "

  snprintf_P(mxtime, sizeof(mxtime), PSTR("%02d" D_HOUR_MINUTE_SEPARATOR "%02d" D_MINUTE_SECOND_SEPARATOR "%02d "), RtcTime.hour, RtcTime.minute, RtcTime.second);

  if (loglevel <= seriallog_level) {
    Serial.printf("%s%s\n", mxtime, log_data);
  }
#ifdef USE_WEBSERVER
  if (Settings.webserver && (loglevel <= Settings.weblog_level)) {
    // Delimited, zero-terminated buffer of log lines.
    // Each entry has this format: [index][log data]['\1']
    if (!web_log_index) web_log_index++;   // Index 0 is not allowed as it is the end of char string
    while (web_log_index == web_log[0] ||  // If log already holds the next index, remove it
           strlen(web_log) + strlen(log_data) + 13 > WEB_LOG_SIZE)  // 13 = web_log_index + mxtime + '\1' + '\0'
    {
      char* it = web_log;
      it++;                                // Skip web_log_index
      it += strchrspn(it, '\1');           // Skip log line
      it++;                                // Skip delimiting "\1"
      memmove(web_log, it, WEB_LOG_SIZE -(it-web_log));  // Move buffer forward to remove oldest log line
    }
    snprintf_P(web_log, sizeof(web_log), PSTR("%s%c%s%s\1"), web_log, web_log_index++, mxtime, log_data);
    if (!web_log_index) web_log_index++;   // Index 0 is not allowed as it is the end of char string
  }
#endif  // USE_WEBSERVER
  if (!global_state.wifi_down && (loglevel <= syslog_level)) { Syslog(); }
}

void AddLog_P(byte loglevel, const char *formatP)
{
  snprintf_P(log_data, sizeof(log_data), formatP);
  AddLog(loglevel);
}

void AddLog_P(byte loglevel, const char *formatP, const char *formatP2)
{
  char message[100];

  snprintf_P(log_data, sizeof(log_data), formatP);
  snprintf_P(message, sizeof(message), formatP2);
  strncat(log_data, message, sizeof(log_data));
  AddLog(loglevel);
}

void AddLogSerial(byte loglevel, uint8_t *buffer, int count)
{
  snprintf_P(log_data, sizeof(log_data), PSTR(D_LOG_SERIAL D_RECEIVED));
  for (int i = 0; i < count; i++) {
    snprintf_P(log_data, sizeof(log_data), PSTR("%s %02X"), log_data, *(buffer++));
  }
  AddLog(loglevel);
}

void AddLogSerial(byte loglevel)
{
  AddLogSerial(loglevel, (uint8_t*)serial_in_buffer, serial_in_byte_counter);
}

void AddLogMissed(char *sensor, uint8_t misses)
{
  snprintf_P(log_data, sizeof(log_data), PSTR("SNS: %s missed %d"), sensor, SENSOR_MAX_MISS - misses);
  AddLog(LOG_LEVEL_DEBUG);
}

/*********************************************************************************************\
 *
\*********************************************************************************************/
