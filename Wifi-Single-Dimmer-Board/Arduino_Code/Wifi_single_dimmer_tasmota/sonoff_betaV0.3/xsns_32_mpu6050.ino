/*
  xsns_32_mpu6050.ino - MPU6050 gyroscope and temperature sensor support for Sonoff-Tasmota

  Copyright (C) 2018  Oliver Welter

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

#ifdef USE_I2C
#ifdef USE_MPU6050
/*********************************************************************************************\
 * MPU6050 3 axis gyroscope and temperature sensor
 *
 * Source: Oliver Welter, with special thanks to Jeff Rowberg
 *
 * I2C Address: 0x68 or 0x69 with AD0 HIGH
\*********************************************************************************************/

#define XSNS_32                          32

#define D_SENSOR_MPU6050                 "MPU6050"

#define MPU_6050_ADDR_AD0_LOW            0x68
#define MPU_6050_ADDR_AD0_HIGH           0x69

uint8_t MPU_6050_address;
uint8_t MPU_6050_addresses[] = { MPU_6050_ADDR_AD0_LOW, MPU_6050_ADDR_AD0_HIGH };
uint8_t MPU_6050_found;

int16_t MPU_6050_ax = 0, MPU_6050_ay = 0, MPU_6050_az = 0;
int16_t MPU_6050_gx = 0, MPU_6050_gy = 0, MPU_6050_gz = 0;
int16_t MPU_6050_temperature = 0;

#include <MPU6050.h>
MPU6050 mpu6050;

void MPU_6050PerformReading()
{
  mpu6050.getMotion6(
    &MPU_6050_ax,
    &MPU_6050_ay,
    &MPU_6050_az,
    &MPU_6050_gx,
    &MPU_6050_gy,
    &MPU_6050_gz
  );

  MPU_6050_temperature = mpu6050.getTemperature();
}

/* Work in progress - not yet fully functional
void MPU_6050SetGyroOffsets(int x, int y, int z)
{
  mpu050.setXGyroOffset(x);
  mpu6050.setYGyroOffset(y);
  mpu6050.setZGyroOffset(z);
}

void MPU_6050SetAccelOffsets(int x, int y, int z)
{
  mpu6050.setXAccelOffset(x);
  mpu6050.setYAccelOffset(y);
  mpu6050.setZAccelOffset(z);
}
*/

void MPU_6050Detect()
{
  if (MPU_6050_found)
  {
    return;
  }

  for (byte i = 0; i < sizeof(MPU_6050_addresses); i++)
  {
    MPU_6050_address = MPU_6050_addresses[i];

    mpu6050.setAddr(MPU_6050_address);
    mpu6050.initialize();

    Settings.flag2.axis_resolution = 2;  // Need to be services by command Sensor32

    MPU_6050_found = mpu6050.testConnection();
  }

  if (MPU_6050_found)
  {
    snprintf_P(log_data, sizeof(log_data), S_LOG_I2C_FOUND_AT, D_SENSOR_MPU6050, MPU_6050_address);
    AddLog(LOG_LEVEL_DEBUG);
  }
}

#ifdef USE_WEBSERVER
const char HTTP_SNS_AX_AXIS[] PROGMEM = "%s{s}%s " D_AX_AXIS "{m}%s{e}";                              // {s} = <tr><th>, {m} = </th><td>, {e} = </td></tr>
const char HTTP_SNS_AY_AXIS[] PROGMEM = "%s{s}%s " D_AY_AXIS "{m}%s{e}";                              // {s} = <tr><th>, {m} = </th><td>, {e} = </td></tr>
const char HTTP_SNS_AZ_AXIS[] PROGMEM = "%s{s}%s " D_AZ_AXIS "{m}%s{e}";                              // {s} = <tr><th>, {m} = </th><td>, {e} = </td></tr>
const char HTTP_SNS_GX_AXIS[] PROGMEM = "%s{s}%s " D_GX_AXIS "{m}%s{e}";                              // {s} = <tr><th>, {m} = </th><td>, {e} = </td></tr>
const char HTTP_SNS_GY_AXIS[] PROGMEM = "%s{s}%s " D_GY_AXIS "{m}%s{e}";                              // {s} = <tr><th>, {m} = </th><td>, {e} = </td></tr>
const char HTTP_SNS_GZ_AXIS[] PROGMEM = "%s{s}%s " D_GZ_AXIS "{m}%s{e}";                              // {s} = <tr><th>, {m} = </th><td>, {e} = </td></tr>
#endif // USE_WEBSERVER

#define D_JSON_AXIS_AX "AccelXAxis"
#define D_JSON_AXIS_AY "AccelYAxis"
#define D_JSON_AXIS_AZ "AccelZAxis"
#define D_JSON_AXIS_GX "GyroXAxis"
#define D_JSON_AXIS_GY "GyroYAxis"
#define D_JSON_AXIS_GZ "GyroZAxis"

void MPU_6050Show(boolean json)
{
  double tempConv = (MPU_6050_temperature / 340.0 + 35.53);

  if (MPU_6050_found) {
    MPU_6050PerformReading();

    char temperature[10];
    dtostrfd(tempConv, Settings.flag2.temperature_resolution, temperature);
    char axis_ax[10];
    dtostrfd(MPU_6050_ax, Settings.flag2.axis_resolution, axis_ax);
    char axis_ay[10];
    dtostrfd(MPU_6050_ay, Settings.flag2.axis_resolution, axis_ay);
    char axis_az[10];
    dtostrfd(MPU_6050_az, Settings.flag2.axis_resolution, axis_az);
    char axis_gx[10];
    dtostrfd(MPU_6050_gx, Settings.flag2.axis_resolution, axis_gx);
    char axis_gy[10];
    dtostrfd(MPU_6050_gy, Settings.flag2.axis_resolution, axis_gy);
    char axis_gz[10];
    dtostrfd(MPU_6050_gz, Settings.flag2.axis_resolution, axis_gz);

    if (json) {
      char json_axis_ax[40];
      snprintf_P(json_axis_ax, sizeof(json_axis_ax), PSTR(",\"" D_JSON_AXIS_AX "\":%s"), axis_ax);
      char json_axis_ay[40];
      snprintf_P(json_axis_ay, sizeof(json_axis_ay), PSTR(",\"" D_JSON_AXIS_AY "\":%s"), axis_ay);
      char json_axis_az[40];
      snprintf_P(json_axis_az, sizeof(json_axis_az), PSTR(",\"" D_JSON_AXIS_AZ "\":%s"), axis_az);
      char json_axis_gx[40];
      snprintf_P(json_axis_gx, sizeof(json_axis_gx), PSTR(",\"" D_JSON_AXIS_GX "\":%s"), axis_gx);
      char json_axis_gy[40];
      snprintf_P(json_axis_gy, sizeof(json_axis_gy), PSTR(",\"" D_JSON_AXIS_GY "\":%s"), axis_gy);
      char json_axis_gz[40];
      snprintf_P(json_axis_gz, sizeof(json_axis_gz), PSTR(",\"" D_JSON_AXIS_GZ "\":%s"), axis_gz);
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s,\"%s\":{\"" D_JSON_TEMPERATURE "\":%s%s%s%s%s%s%s,\"}"),
        mqtt_data, D_SENSOR_MPU6050, temperature, json_axis_ax, json_axis_ay, json_axis_az, json_axis_gx, json_axis_gy, json_axis_gz);
#ifdef USE_DOMOTICZ
      DomoticzTempHumSensor(temperature, 0);
#endif // USE_DOMOTICZ
#ifdef USE_WEBSERVER
    } else {
      snprintf_P(mqtt_data, sizeof(mqtt_data), HTTP_SNS_TEMP, mqtt_data, D_SENSOR_MPU6050, temperature, TempUnit());
      snprintf_P(mqtt_data, sizeof(mqtt_data), HTTP_SNS_AX_AXIS, mqtt_data, D_SENSOR_MPU6050, axis_ax);
      snprintf_P(mqtt_data, sizeof(mqtt_data), HTTP_SNS_AY_AXIS, mqtt_data, D_SENSOR_MPU6050, axis_ay);
      snprintf_P(mqtt_data, sizeof(mqtt_data), HTTP_SNS_AZ_AXIS, mqtt_data, D_SENSOR_MPU6050, axis_az);
      snprintf_P(mqtt_data, sizeof(mqtt_data), HTTP_SNS_GX_AXIS, mqtt_data, D_SENSOR_MPU6050, axis_gx);
      snprintf_P(mqtt_data, sizeof(mqtt_data), HTTP_SNS_GY_AXIS, mqtt_data, D_SENSOR_MPU6050, axis_gy);
      snprintf_P(mqtt_data, sizeof(mqtt_data), HTTP_SNS_GZ_AXIS, mqtt_data, D_SENSOR_MPU6050, axis_gz);
#endif // USE_WEBSERVER
    }
  }
}

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

boolean Xsns32(byte function)
{
  boolean result = false;

  if (i2c_flg) {
    switch (function) {
      case FUNC_PREP_BEFORE_TELEPERIOD:
        MPU_6050Detect();
        break;
      case FUNC_EVERY_SECOND:
        if (tele_period == Settings.tele_period -3) {
          MPU_6050PerformReading();
        }
        break;
      case FUNC_JSON_APPEND:
        MPU_6050Show(1);
        break;
#ifdef USE_WEBSERVER
      case FUNC_WEB_APPEND:
        MPU_6050Show(0);
        MPU_6050PerformReading();
        break;
#endif // USE_WEBSERVER
    }
  }
  return result;
}

#endif // USE_MPU6050
#endif // USE_I2C
