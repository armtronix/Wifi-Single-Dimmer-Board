/*Code for Wifi Single triac 2 amps mini board
The Board has one Triacs and it is only used in ON/OFF mode

This code is for Atmega328p 
Firmware Version: 0.2
Hardware Version: 0.2

Code Edited By :Naren N Nayak
Date: 30/10/2017
Last Edited By:Naren N Nayak
Date: 24/10/2017

*/ 


//Dimmer pin no.
#define NON_DIMMABLE_TRIAC 8 //Gpio 7       

//manual switch 
#define SWITCH_INPIN A0 //switch / pot input 



/*Serial Data variables*/
String serialReceived;
String serialReceived1;




void setup() 
{

  Serial.begin(115200);
  Serial.println("WiFi-Single channel 2A-Dimmer board on/off mode");
  pinMode(NON_DIMMABLE_TRIAC, OUTPUT); //Dimmer output
  pinMode(SWITCH_INPIN, INPUT); //manual switch 1 input
}






void loop() 
{
/*############### Uart Data ###########################*/
  
  if (Serial.available() > 0) 
  {   // is a character available
    
    serialReceived = Serial.readStringUntil('\n');
    Serial.println(serialReceived);
    
    if (serialReceived.substring(0, 9) == "Dimmer:99")
    {
      serialReceived1 = "R_1 switched via web request to 1";     
    }

     if (serialReceived.substring(0, 8) == "Dimmer:0")
    {
      serialReceived1 = "R_1 switched via web request to 0";     
    }

  }


  /*##################### Non Dimmable Triac 2##############################*/
  
  if (((serialReceived1.substring(0, 33) == "R_1 switched via web request to 1") && (!(digitalRead(SWITCH_INPIN)))) || ((!(serialReceived1.substring(0, 33) == "R_1 switched via web request to 1")) && ((digitalRead(SWITCH_INPIN))))) //exor logic
  {
    if(digitalRead(NON_DIMMABLE_TRIAC)==HIGH)
    {
      Serial.println("Load is OFF");
    }
    digitalWrite(NON_DIMMABLE_TRIAC, LOW);
    
  }
  else
  {
    if(digitalRead(NON_DIMMABLE_TRIAC)==LOW)
    {
      Serial.println("Load  is ON");
    }
    digitalWrite(NON_DIMMABLE_TRIAC, HIGH);
   
  }


  

  
  
}
