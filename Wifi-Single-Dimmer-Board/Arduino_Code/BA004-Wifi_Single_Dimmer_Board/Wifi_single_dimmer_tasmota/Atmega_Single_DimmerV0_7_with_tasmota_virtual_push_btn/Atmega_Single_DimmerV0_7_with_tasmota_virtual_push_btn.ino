/*Code for Wifi Single triac 2 amps mini board
The Board has one Triacs and it is dimmable 

This code is for Atmega328p 
Firmware Version: 0.7
Hardware Version: 0.3

Code Edited By :Karthik S B
Date: 29/05/2018  //Description  modified the frequency step to 85 and dimming width to 100
Date: 16/07/2021  //Gpio for vitrual switch insted of pot ~ read type also was not declared as input in setup 
Last Edited By:Naren N Nayak


*/ 
#include  <TimerOne.h>
#define version_no "FVer 0.7 ,HVer 0.3" //note that fver 0.6 with few changes dated 16/07/2021 is fver 0.7 some hardware may have 0.6 on serial print date of sale from 15/07/21 may have this serial print 
//Dimmer pin no.
#define DIMMABLE_TRIAC 8 //Gpio 7       

//manual switch 
#define SWITCH_INPIN A0 //switch / pot input 

/* ZCD */

#define ZCD_INT 0  //Arduino GPIO2 
#define Dimmer_width 100 //100
#define HUMANPRESSDELAY 50 // the delay in ms untill the press should be handled as a normal push by human. Button debounce.

unsigned long count_regulator = 0; //Button press time counter
unsigned long dimval = 0; //Button press time counter
int button_press_flag = 1;
byte tarBrightness = 0;
byte curBrightness = 0;

int dimming = 100;  //100
int freqStep = 85;//85,82,80 //75*5 as prescalar is 16 for 80MHZ //77 for the led
/*Serial Data variables*/
String serialReceived;
String Dimmer_value_temp;
String Dimmer_value;
String regulator_value_temp;
/*POT Variable */
String regulator_value;

/*ZCD Variables */

volatile int dim_value = 0;
volatile boolean zero_cross = 0;
volatile int int_regulator=0;
volatile int int_regulator_temp;
volatile int i=1;
/*Flags for Dimmer virtual switch concept */
volatile boolean dimmer_value_changed =false; 
volatile boolean regulator_value_changed =false;
volatile boolean dimmer_status =false;
volatile boolean mqttconnected =false;
int dimvalue;

void setup() {

  Serial.begin(115200);
 // Serial.println("WiFi-Single channel 2A-Dimmer board");
 // Serial.println(version_no);
  pinMode(DIMMABLE_TRIAC, OUTPUT); //Dimmer output
  pinMode(SWITCH_INPIN, INPUT);//gpio input for push button dimmer added on 16/07/21
  attachInterrupt(ZCD_INT, zero_cross_detect, CHANGE);    // Attach an Interupt to Pin 2 (interupt 0) for Zero Cross Detection
  Timer1.initialize(freqStep);                      // Initialize TimerOne library for the freq we need
  Timer1.attachInterrupt(dim_check, freqStep);
  
//   while (!mqttconnected) 
// {   // is a character available
//    serialReceived = Serial.readStringUntil('\n');
//    if(serialReceived.substring(0, 9) == "CONNECTED")
//    {
//      mqttconnected =true;
//      dimmer_status = true;
//      serialReceived="";
//    }
//
//  }
//dimmer_status = true;
}



void btn_handle()
{
  if (count_regulator <= 9)
  {
    count_regulator = 0;
    tarBrightness = count_regulator;
  }

  if (!digitalRead(SWITCH_INPIN)) //gpio input inverted while reading for push button dimmer added on 16/07/21
  {
    if (button_press_flag == 1)
    {
      button_press_flag = 0;
      if (count_regulator <= 9)
      {
        tarBrightness = count_regulator + 10;
        count_regulator = count_regulator + 10;
      }
      else
      {
        count_regulator = count_regulator + 10;
        tarBrightness = count_regulator;
      }
      if (count_regulator <= 90)
      {
        //Serial.print("Reg VAL:");
        //Serial.println(count_regulator);
        int_regulator = count_regulator;
      }
      else if (count_regulator <= 100 && count_regulator > 90 )
      {
        dimval = count_regulator - 1;
        //Serial.print("Reg VAL:");
        //Serial.println(dimval);
        int_regulator = dimval;
      }
      else
      {
        count_regulator = 0;
        //Serial.print("Reg VAL:");
        //Serial.println(count_regulator);
        int_regulator = count_regulator;
      }
    }
  }
  else
  {
    button_press_flag = 1;
    if (count_regulator > 1 && count_regulator < HUMANPRESSDELAY / 5)
    {
      if (count_regulator <= 99)
      {
        //Serial.println(count_regulator);
      }
      else
      {
        count_regulator = 0;
      }
    }
  }
}


/*ZCD Interrupt Function*/
void zero_cross_detect() 
{
  zero_cross = true;               // set the boolean to true to tell our dimming function that a zero cross has occured
  dim_value = 0;
  digitalWrite(DIMMABLE_TRIAC, LOW);      // turn off TRIAC (and AC)
}

/*Timer Interrupt Function used to trigger the triac for Dimming*/
void dim_check() 
{
  /*For Dimmer */
  if (zero_cross == true) 
  {
    if (dim_value >= dimming) 
    {
      digitalWrite(DIMMABLE_TRIAC, HIGH); // turn on Triac 
      dim_value = 0; // reset time step counter
      zero_cross = false; //reset zero cross detection
    }
    else 
    {
      dim_value++; // increment time step counter
    }
  }
}


void loop() 
{



  btn_handle();

  i++;

  if (i >= 100)
  {
    regulator_value_temp = "Dimmer:" + String((int_regulator));
    i = 0;
  }

/*############### Flag setting for Dimmable Triac through Pot ###############*/
  
  if(regulator_value_temp!=regulator_value)
  {
    regulator_value=regulator_value_temp;
    regulator_value_changed =true;
  }
  else
  {
    regulator_value_changed =false;
  }


/*############### Uart Data ###########################*/
  
  if (Serial.available() > 0) 
  {   // is a character available
    
    serialReceived = Serial.readStringUntil('\n');
//    Serial.println(serialReceived);
     if (serialReceived.substring(0, 6) == "Status")
    {
      dimmer_status = true;     
    }
    if (serialReceived.substring(0, 7) == "Dimmer:")
    {
      Dimmer_value_temp = serialReceived;     
    }
  }

/*################## Flag setting for Dimmable Triac through uart ##################################*/

  if( Dimmer_value_temp!=Dimmer_value)
  {
    Dimmer_value=Dimmer_value_temp;
    dimmer_value_changed =true;
  }
  else 
  {
    dimmer_value_changed =false;
  }

  
/*####################### Dimmable Triac ##################################*/
  
  if (Dimmer_value.substring(0, 7) == "Dimmer:" && dimmer_value_changed == true )
  {
   // int sensorValue = map(Dimmer_value.substring(7, 9).toInt(), 0, 99, 0, 128);
   // Serial.println("map value sensor "+String(sensorValue));
//    dimming = Dimmer_width - sensorValue;
    dimming = Dimmer_width - Dimmer_value.substring(7, 9).toInt();
    delay(5);
 //   Serial.println("Uart value to "+Dimmer_value);
    dimvalue = Dimmer_value.substring(7, 9).toInt();
  }

  if (regulator_value.substring(0, 7) == "Dimmer:" && regulator_value_changed == true )
  {
  
    dimming = Dimmer_width - regulator_value.substring(7, 9).toInt();
    delay(5);
    //Serial.println("Regulator value to "+regulator_value); //commented serial read added on 16/07/21
    dimvalue = regulator_value.substring(7, 9).toInt();
  }

if(dimmer_status == true)
  {
   dim_status(); 
   dimmer_status = false;
  
  }
}

void dim_status() 
{
  Serial.println("Status:"+String(dimvalue)+",0,"+regulator_value.substring(7,9)+",0"); 
}
