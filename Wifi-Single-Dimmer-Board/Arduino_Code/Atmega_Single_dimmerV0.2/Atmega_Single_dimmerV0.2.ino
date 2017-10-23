#include  <TimerOne.h>

//Dimmer pin no.
#define Dimmer 8 //Gpio 7       


//manual switch
//#define SWITCH_INPIN1 A0 //switch 1

// Variables will change:
//int buttonPushCounter = 0;   // counter for the number of button presses
//int buttonState = 0;         // current state of the button
//int lastButtonState = 0;     // previous state of the button

int switch_status1;
String serialReceived;
String Dimmer_value;

int freqStep = 75;//75*5 as prescalar is 16 for 80MHZ
volatile int i = 0;
int dimming = 115;
volatile boolean zero_cross = 0;
//int Dimmer = 6;
int inc = 1;


void setup() {

  Serial.begin(115200);
  Serial.println("WIFI SINGLE DIMMER V0.2");
  pinMode(Dimmer, OUTPUT); //Dimmer output
  //     pinMode(SWITCH_INPIN1, INPUT); //manual switch 1 input

  attachInterrupt(0, zero_cross_detect, CHANGE);    // Attach an Interupt to Pin 2 (interupt 0) for Zero Cross Detection
  Timer1.initialize(freqStep);                      // Initialize TimerOne library for the freq we need
  Timer1.attachInterrupt(dim_check, freqStep);
  //  attachInterrupt(digitalPinToInterrupt(Dimmer), Dimm, CHANGE);
}

//void Dimm()  //
//{
//   if(dimming == 0)
//    {
//    digitalWrite(Dimmer,LOW);
//    }
//  else if (dimming == 1)
//  {
//    digitalWrite(Dimmer,LOW);
//
//  }
//}

void zero_cross_detect() {
  zero_cross = true;               // set the boolean to true to tell our dimming function that a zero cross has occured
  i = 0;
  digitalWrite(Dimmer, LOW);      // turn off TRIAC (and AC)
}

void dim_check() {
  if (zero_cross == true) {
    if (i >= dimming) {
      digitalWrite(Dimmer, HIGH); // turn on light

      i = 0; // reset time step counter
      zero_cross = false; //reset zero cross detection
    }
    else {
      i++; // increment time step counter
    }
  }
}


void loop() {

  //   buttonState = digitalRead(SWITCH_INPIN1);
  //   if (buttonState != lastButtonState) {
  //
  //    if (buttonState == HIGH) {
  //      Serial.println("low");
  //      Dimmer_value = "Dimmer:0";
  //    } else {
  //      Serial.println("high");
  //      Dimmer_value = "Dimmer:99";
  //    }
  //    // Delay a little bit to avoid bouncing
  //    delay(50);
  //  }
  //
  //  lastButtonState = buttonState;




  //        Serial.println(Serial.available());

  if (Serial.available() > 0) {   // is a character available
    serialReceived = Serial.readStringUntil('\n');
    Serial.println(serialReceived);
    if (serialReceived.substring(0, 7) == "Dimmer:")

      Dimmer_value = serialReceived;

  }



  //Traic_out-----------------------------

  if (Dimmer_value.substring(0, 7) == "Dimmer:")
  {
    dimming = 115 - Dimmer_value.substring(7, 9).toInt();
    //      Serial.println(state_dimmer);
    //         dimming =127-state_dimmer;
    //         Serial.println(dimming);
    delay(5);

  }

}


