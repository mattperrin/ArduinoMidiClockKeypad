/* 
 * Arduino MIDI Clock Keypad by Matthew Perrin is licensed under a 
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
 * Based on a work at http://github.com/mattperrin.
 * Twitter.com/mattperrin
 * 
  The circuit:
  LCD RS pin to digital pin 8
  LCD Enable pin to digital pin 9
  LCD D4 pin to digital pin 4
  LCD D5 pin to digital pin 5
  LCD D6 pin to digital pin 6
  LCD D7 pin to digital pin 7
  LCD BL pin to digital pin 10
  KEY pin to analog pin 0        (14)  THIS IS THE BUTTON PIN
  http://www.electronics.dit.ie/staff/tscarff/Music_technology/midi/midi_note_numbers_for_octaves.htm
*/

#include <MIDI.h>
#include <Wire.h>
#include <LiquidCrystal.h>
LiquidCrystal lcd(8, 13, 9, 4, 5, 6, 7);

byte midi_start = 0xfa;
byte midi_stop = 0xfc;
byte midi_clock = 0xf8;
byte midi_continue = 0xfb;

unsigned long currentMillis;
unsigned long previousMillis;
unsigned long differenceTiming;
long buttonTimerReset = 0;

bool buttonDown = false;

int bpmInterval;
int midiInterval;
int arduinoInterval;
unsigned long beatCounter;
unsigned long subCounter;

int bpmTempo = 60;
int bpmPotPin = A7;
int ledPin = 52;
int bpmPotValue = 0;
bool fineAdjustment = false;
bool fineAdjustmentModified = false;

//BPM POT SMOOTHING
const int numReadings = 30;
int readings[numReadings];      // the readings from the analog input
int readIndex = 0;              // the index of the current reading
int total = 0;                  // the running total
int average = 0;
int oldPotValue = 0;

/* Stop
   Start
   Playing
   Pause
   Continue
*/
String currentState = "Idle";
bool currentStateChanged = true;
bool debugFlag = false;


MIDI_CREATE_DEFAULT_INSTANCE();

void setup() {
  delay(1000);

  pinMode(ledPin, OUTPUT);

  MIDI.begin();
  Serial.begin(115200);
  Serial1.begin(115200);
  Serial2.begin(115200);
  Serial3.begin(115200);
  lcd.clear();
  lcd.begin(16, 2);
  lcd.setCursor(0, 0);
  lcd.print("MIDI Clock v1.0");
  lcd.setCursor(0, 1);
  lcd.print("@mattperrin");

  CalculateBpm(bpmTempo);

  // initialize all the readings to 0:
  for (int thisReading = 0; thisReading < numReadings; thisReading++) {
    readings[thisReading] = 0;
  }
  currentState = "Idle";
  delay(1000);

  // DEBUGGING TO SERIAL MONITOR FLAG
  debugFlag = false;

  lcd.clear();
}


void loop() {
  //DebugMsg("++++++++++++++++++++++++++");
  //DebugMsg("LOOP");

  currentMillis = millis();
  differenceTiming = currentMillis - previousMillis;

  // RESET BUTTON
  if (buttonTimerReset < -10000) {
    buttonTimerReset = 0;
  }
  buttonTimerReset -= differenceTiming;

  // CHECK BUTTONS
  if (buttonTimerReset < 0)
  {
    //DebugMsg("DETECT");
    DetectKeypress();
  }

  if (currentState == "Stop")
  {
    //DebugMsg("STOP");
    Serial1.write(midi_stop);
    Serial2.write(midi_stop);
    Serial3.write(midi_stop);
    digitalWrite(ledPin, LOW);
    subCounter = 0;
    currentState = "Idle";
    currentStateChanged = true;
  }
  //else if (currentState == "Pause")
  //{
    //DO NOTHING
  //}
  else if (currentState == "Play" or currentState == "Start" or currentState == "Continue")
  {
    //DebugMsg("PLAYING " + (String)bpmTempo);
    if (bpmTempo > 0)
    {
      if (differenceTiming >= arduinoInterval)
      {
        //DebugMsg("***************************************");
        //DebugMsg("TIMER TRIGGER");
        //DebugMsg("***************************************");
        if (subCounter == 8) {
          subCounter = 0;
        }

        if (currentState == "Play")
        {
          //DebugMsg("PLAY START");
          Serial1.write(midi_clock);
          Serial2.write(midi_clock);
          Serial3.write(midi_clock);
        }
        else if (currentState == "Continue")
        {
          //DebugMsg("Continue START");
          Serial1.write(midi_continue);
          Serial2.write(midi_continue);
          Serial3.write(midi_continue);
          currentState = "Play";
          currentStateChanged = true;
        }
        else if (currentState == "Start")
        {
          //DebugMsg("Start START");
          Serial1.write(midi_start);
          Serial2.write(midi_start);
          Serial3.write(midi_start);
          currentState = "Play";
          currentStateChanged = true;
          subCounter = 0;
        }

        if (subCounter == 0) {
          digitalWrite(ledPin, HIGH);
        }
        if (subCounter == 4) {
          digitalWrite(ledPin, LOW);
        }

        subCounter += 1;
        previousMillis = currentMillis;
      }
    }
  }

  bpmPotValue = (int)analogRead(bpmPotPin);
  total = total - readings[readIndex];
  readings[readIndex] = bpmPotValue;
  total = total + readings[readIndex];
  readIndex = readIndex + 1;
  if (readIndex >= numReadings) {
    readIndex = 0;
  }
  average = (total / numReadings);
  int newValue = map(average, 0, 1023, 240, 1);

  if (abs(bpmPotValue - oldPotValue) > 5 or abs(bpmPotValue - oldPotValue) < -5) {
    fineAdjustment = false;
    oldPotValue = bpmPotValue;
  }

  if (fineAdjustment == false)
  {
    if (newValue != bpmTempo)
    {
      CalculateBpm(newValue);
    }
  }
  else
  {
    if (fineAdjustmentModified == true)
    {
      CalculateBpm(bpmTempo);
      fineAdjustmentModified = false;
    }
  }

  DisplayCurrentBpm();
  DisplayCurrentState();
}


void DetectKeypress()
{
  // right = 0, up 141, left 503, down 326, select 740, idle 1023
  int adc_key_in = analogRead(0);

  if (adc_key_in > 1000) { // IDLE
    return;
  }

  if (buttonDown == false)
  {
    if (adc_key_in > 710) { //SELECT button
      currentState = "Stop";
      currentStateChanged = true;
      buttonTimerReset = 500;
    }
    else if (adc_key_in > 470) { //LEFT
      currentState = "Start";
      currentStateChanged = true;
      buttonTimerReset = 500;
    }
    else if (adc_key_in > 290) { //DOWN
      bpmTempo -= 1;
      fineAdjustment = true;
      fineAdjustmentModified = true;
      buttonTimerReset = 500;
    }
    else if (adc_key_in > 110) { //UP
      bpmTempo += 1;
      fineAdjustment = true;
      fineAdjustmentModified = true;
      buttonTimerReset = 500;
    }
    else if (adc_key_in >= 0) { // RIGHT
      if (currentState == "Play")
      {
        currentState = "Pause";
        currentStateChanged = true;
        buttonTimerReset = 1000;
      }
      else if (currentState == "Pause")
      {
        currentState = "Continue";
        currentStateChanged = true;
        buttonTimerReset = 1000;
      }
    }
  }
}

void CalculateBpm(int bpm)
{
  //DebugMsg("CALC BPM:" + (String)bpm);
  bpmInterval = (60000 / bpm);
  midiInterval = bpmInterval / 24;
  arduinoInterval = bpmInterval / 8;
  bpmTempo = bpm;
}

void DisplayCurrentState()
{
  //DebugMsg("UPDATE STATE:" + currentState);
  if (currentStateChanged == true)
  {
    lcd.setCursor(0, 0);
    lcd.print("Status: " + currentState + "     ");
  }
}

void DisplayCurrentBpm()
{
  lcd.setCursor(0, 1);
  lcd.print("BPM: " + (String)bpmTempo + "            ");
}


void DebugMsg(String msg)
{
  if (debugFlag == true)
  {
    Serial.println(msg);
  }
}


