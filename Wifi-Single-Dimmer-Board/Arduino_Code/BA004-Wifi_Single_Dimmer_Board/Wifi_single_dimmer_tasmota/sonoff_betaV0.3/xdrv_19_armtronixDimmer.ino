/*
  xdrv_16_armtronixdimmer.ino - Armtronix dimmer support for Sonoff-Tasmota

  Copyright (C) 2018  digiblur, Joel Stein and Theo Arends

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



#ifdef USE_ARMTRONIX_SINGLE_DIMMER

#define XDRV_19                19

#include <TasmotaSerial.h>

TasmotaSerial *ArmtronixSerial2 = nullptr;
//
boolean armtronix_ignore_dim2 = false;            // Flag to skip serial send to prevent looping when processing inbound states from the faceplate interaction
int8_t armtronix_wifi_state2 = -2;                // Keep MCU wifi-status in sync with WifiState()
int8_t armtronix_dimState2[1];                   //Dimmer state values.
int8_t armtronix_knobState2[1];                   //Dimmer state values.


/*********************************************************************************************\
 * Internal Functions
\*********************************************************************************************/



void LightSerial3Duty(uint8_t duty1)
{
  if (ArmtronixSerial2 && !armtronix_ignore_dim2) {
  duty1 = ((float)duty1)/2.575757; //max 99
 // duty2 = ((float)duty2)/2.575757; //max 99
  armtronix_dimState2[0] = duty1;
//  armtronix_dimState2[1] = duty2;
  ArmtronixSerial2->print("Dimmer:");
  ArmtronixSerial2->println(duty1);
//  ArmtronixSerial->print("Dimmer2:");
//  ArmtronixSerial->println(duty2);

    snprintf_P(log_data, sizeof(log_data), PSTR( "ARM: Send Serial Packet Dim Values=%d,%d"), armtronix_dimState2[0]);
    AddLog(LOG_LEVEL_DEBUG);

  } else {
    armtronix_ignore_dim2 = false;
    snprintf_P(log_data, sizeof(log_data), PSTR( "ARM: Send Dim Level skipped due to already set. Value=%d,%d"), armtronix_dimState2[0]);
    AddLog(LOG_LEVEL_DEBUG);

  }
}

void ArmtronixRequestState2(){
  if(ArmtronixSerial2) {
    // Get current status of MCU
    snprintf_P(log_data, sizeof(log_data), "TYA: Request MCU state");
    AddLog(LOG_LEVEL_DEBUG);
    ArmtronixSerial2->println("Status");
    
  }
}

/*********************************************************************************************\
 * API Functions
\*********************************************************************************************/

boolean ArmtronixModuleSelected2()
{
  light_type = LT_SERIAL;
  return true;
}

void ArmtronixInit2()
{
  armtronix_dimState2[0] = -1;
 // armtronix_dimState[1] = -1;
  armtronix_knobState2[0] = -1;
 // armtronix_knobState2[1] = -1;
  ArmtronixSerial2 = new TasmotaSerial(pin[GPIO_RXD], pin[GPIO_TXD], 2);
  if (ArmtronixSerial2->begin(115200)) {
    if (ArmtronixSerial2->hardwareSerial()) { ClaimSerial(); }
     ArmtronixSerial2->println("SerialClaimed");
    ArmtronixSerial2->println("Status");
  }
}

void ArmtronixSerialInput2()
{
  String answer;
  int8_t newDimState[1];
  uint8_t temp;
 // uint8_t temp2 = 100;
  int commaIndex;
  char scmnd[20];  
  if (ArmtronixSerial2->available()) {
    yield();
    answer = ArmtronixSerial2->readStringUntil('\n');
    if(answer.substring(0,7) == "Status:"){
      commaIndex = 6;
      for(int i =0;i<1;i++){
        newDimState[i] = answer.substring(commaIndex+1,answer.indexOf(',',commaIndex+1)).toInt();
        if(newDimState[i] != armtronix_dimState2[i]){
          temp = ((float)newDimState[i])*1.01010101010101; //max 255
          armtronix_dimState2[i] = newDimState[i];
          armtronix_ignore_dim2 = true;
          snprintf_P(scmnd, sizeof(scmnd), PSTR(D_CMND_CHANNEL "%d %d"),i+1, temp);
          ExecuteCommand(scmnd,SRC_SWITCH);
          snprintf_P(log_data, sizeof(log_data), PSTR("ARM: Send CMND_CHANNEL=%s"), scmnd );
          AddLog(LOG_LEVEL_DEBUG);
        }
          commaIndex = answer.indexOf(',',commaIndex+1);
      }
      armtronix_knobState2[0] = answer.substring(commaIndex+1,answer.indexOf(',',commaIndex+1)).toInt();
     // commaIndex = answer.indexOf(',',commaIndex+1);
     // armtronix_knobState2[1] = answer.substring(commaIndex+1,answer.indexOf(',',commaIndex+1)).toInt();
    }
  }
}

void ArmtronixSetWifiLed2(){
    uint8_t wifi_state = 0x02;
    switch(WifiState()){
      case WIFI_SMARTCONFIG:
        wifi_state = 0x00;
        break;
      case WIFI_MANAGER:
      case WIFI_WPSCONFIG:
        wifi_state = 0x01;
        break;
      case WIFI_RESTART:
        wifi_state =  0x03;
        break;
    }

    snprintf_P(log_data, sizeof(log_data), "ARM: Set WiFi LED to state %d (%d)", wifi_state, WifiState());
    AddLog(LOG_LEVEL_DEBUG);
   
    char state = '0' + (wifi_state & 1 > 0);
//    ArmtronixSerial2->print("Setled:");
//    ArmtronixSerial2->write(state);
//    ArmtronixSerial2->write(',');
    state = '0' + (wifi_state & 2 > 0);
//    ArmtronixSerial2->write(state);
//    ArmtronixSerial2->write(10);
    armtronix_wifi_state2 = WifiState();


}


/*********************************************************************************************\
 * Interface
\*********************************************************************************************/
bool flip2;

boolean Xdrv19(byte function)
{
  boolean result = false;

  if (ARMTRONIX_SINGLE_DIMMER == Settings.module) {
    switch (function) {
      case FUNC_MODULE_INIT:
        result = ArmtronixModuleSelected2();
        break;
      case FUNC_INIT:
        ArmtronixInit2();
        break;
      case FUNC_LOOP:
        if (ArmtronixSerial2) { ArmtronixSerialInput2(); }
        break;
      case FUNC_EVERY_SECOND:
        if(ArmtronixSerial2){
          if (armtronix_wifi_state2!=WifiState()) { ArmtronixSetWifiLed2(); }
          if(uptime&1){
            ArmtronixSerial2->println("Status");
          }
        }
        break;
    }
  }
  return result;
}

#endif  // USE_ARMTRONIX_SINGLE_DIMMER
