/*Code for Wifi Single triac 2 amps mini board
The Board has one Triacs and it is dimmable 

This code is for Atmega328p 
Firmware Version: 0.6
Hardware Version: 0.3

Code Edited By :Karthik S B
Date: 29/05/2018  //Description  modified the frequency step to 85 and dimming width to 100
Last Edited By:Naren N Nayak
Date: 09/06/2018

*/ 
#include  <TimerOne.h>
#define version_no "FVer 0.6 ,HVer 0.3" 
//Dimmer pin no.
#define DIMMABLE_TRIAC 8 //Gpio 7       

//manual switch 
#define SWITCH_INPIN A0 //switch / pot input 

/* ZCD */

#define ZCD_INT 0  //Arduino GPIO2 
#define Dimmer_width 100 //100

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

int_regulator=analogRead(SWITCH_INPIN);  // added on 06/03/19          
   int data ;
  // Serial.println(int_regulator);
  if (int_regulator >=850)
   {
    data = 99;//  x* (99/1024)  x is adc data
    //Serial.println(data);
   }
   else if (int_regulator >=700)// || (int)int_regulator <1000)
   {
    data =90; //  x* (99/1024)  x is adc data
    //Serial.println(data);
   }
   else if (int_regulator >=530)// || (int)int_regulator <1000)
   {
    data =80; //  x* (99/1024)  x is adc data
    //Serial.println(data);
   }
   else if (int_regulator >=420)// || (int)int_regulator <1000)
   {
    data =70; //  x* (99/1024)  x is adc data
    //Serial.println(data);
   }
   else if (int_regulator >=310)// || (int)int_regulator <600)
   {
    data = 60;//  x* (99/1024)  x is adc data
    //Serial.println(data);
   }
   else if (int_regulator >=270)//|| (int)int_regulator <400)
   {
    data = 50;//  x* (99/1024)  x is adc data
    //Serial.println(data);
   }
      else if (int_regulator >=230)//|| (int)int_regulator <100)
   {
    data = 40;//  x* (99/1024)  x is adc data
    //Serial.println(data);
   }
   else if (int_regulator >=190)//|| (int)int_regulator <100)
   {
    data = 30;//  x* (99/1024)  x is adc data
    //Serial.println(data);
   }
    else if (int_regulator >=170)//|| (int)int_regulator <100)
   {
    data = 20;//  x* (99/1024)  x is adc data
    //Serial.println(data);
   }
    else if (int_regulator >=161)//|| (int)int_regulator <100)
   {
    data = 10;//  x* (99/1024)  x is adc data
    //Serial.println(data);
   }
   else if (int_regulator <160)
   {
    data = 0;//  x* (99/1024)  x is adc data
    //Serial.println(data);
   }
   regulator_value_temp="Dimmer:"+String(data);
   //Serial.println("Regulator value to "+regulator_value_temp);
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
    if (serialReceived.substring(0, 8) == "Dimmer1:")
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
  
  if (Dimmer_value.substring(0, 8) == "Dimmer1:" && dimmer_value_changed == true )
  {
   // int sensorValue = map(Dimmer_value.substring(7, 9).toInt(), 0, 99, 0, 128);
   // Serial.println("map value sensor "+String(sensorValue));
//    dimming = Dimmer_width - sensorValue;
    dimming = Dimmer_width - Dimmer_value.substring(8, 10).toInt();
    delay(5);
 //   Serial.println("Uart value to "+Dimmer_value);
    dimvalue = Dimmer_value.substring(8, 10).toInt();
  }

  if (regulator_value.substring(0, 7) == "Dimmer:" && regulator_value_changed == true )
  {
  
    dimming = Dimmer_width - regulator_value.substring(7, 9).toInt();
    delay(5);
 //   Serial.println("Regulator value to "+regulator_value);
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
