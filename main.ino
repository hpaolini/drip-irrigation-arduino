#include <TimeLib.h>
#include <Wire.h>
#include <DS1307RTC.h>  // a basic DS1307 library that returns time as a time_t
#include <TimeAlarms.h>
#include <Bounce2.h> // prevent button debouncing problem

//#define DEBUG // uncomment for debugging

#ifdef DEBUG
  #define DEBUG_PRINT(x) Serial.print (x)
  #define DEBUG_PRINTLN(x) Serial.println (x)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
#endif

#define buttonPIN 4
#define ledPIN 13
#define enablePIN 10

// these are the pins from the h-bridge that we'll use to inverse the polarity
#define valvePIN1 14
#define valvePIN2 15

Bounce pushButton = Bounce();
bool isValveOpen = false;
bool isFirstRun = true;

void setup () {
  Serial.begin(9600);
  setSyncProvider(RTC.get);   // the function to get the time from the RTC
  if (timeStatus() != timeSet) {
     DEBUG_PRINTLN("Unable to sync with the RTC");
  } else {
     DEBUG_PRINTLN("RTC has set the system time");
  }

  pinMode(buttonPIN, INPUT_PULLUP);
  pinMode(ledPIN,OUTPUT);
  pinMode(enablePIN, OUTPUT);
  pinMode(valvePIN1, OUTPUT);
  pinMode(valvePIN2, OUTPUT);
  closeValve();
  
  // after setting up the button, setup the object
  pushButton.attach(buttonPIN);
  pushButton.interval(5);

  // we'll run the first cycle to setup the alarm
  schedule();
}

void loop () {
  if (Serial.available()) {
    time_t t = processSyncMessage();
    if (t != 0) {
      RTC.set(t);   // set the RTC and the system time to the received value
      setTime(t);          
    }
  }

  pushButton.update();
  int value = pushButton.read();
  
  if (value == HIGH) {
    digitalWrite(ledPIN, HIGH);
    openValve(30);
  } else {
    digitalWrite(ledPIN, LOW);
  }
 
  Alarm.delay(500); 
}

void schedule () {
  switch (month()) {
    // winter - water every week for 10 minutes
    case 12:
    case 1:
    case 2:
      openValve(10);
      Alarm.alarmOnce(dowSunday, 7, 30, 0, schedule);
      break;
        
    // spring - water everyday for 40 minutes
    case 3:
    case 4:
    case 5:
    case 9:
      openValve(40);
      Alarm.alarmOnce(7, 30, 0, schedule);
      break;
      
    // summer - water twice a day for 40 minutes
    case 6:
    case 7:
    case 8:
      openValve(40);
      if (hour() < 12) {
        Alarm.alarmOnce(21, 30, 0, schedule);
      } else {
        Alarm.alarmOnce(7, 30, 0, schedule);
      }
      break;
        
    // autumn - water every week for 40 minutes
    //case 9: // september is still hot and sunny in italy, moved to spring schedule
    case 10:
    case 11:
    default:
      openValve(40);
      Alarm.alarmOnce(dowSunday, 7, 30, 0, schedule);
      break;
  }
}

void openValve (int minutes) {
  if (isValveOpen) {
    return;
  } else if (isFirstRun) {
    isFirstRun = false;
    return;
  }

  digitalWrite(enablePIN, HIGH);
  digitalWrite(valvePIN1, HIGH);
  digitalWrite(valvePIN2, LOW);
  isValveOpen = true;
  DEBUG_PRINTLN("Valve opened");
  
  // make sure to close the valve after given minutes
  Alarm.timerOnce(minutes * 60, closeValve);
}

void closeValve () {
  if (digitalRead(enablePIN) == LOW) {
    digitalWrite(enablePIN, HIGH);
  }
  
  digitalWrite(valvePIN1, LOW);
  digitalWrite(valvePIN2, HIGH);
  isValveOpen = false;
  DEBUG_PRINTLN("Valve closed");

  // the valve needs at least 5 seconds to fully close/open
  Alarm.delay(5000);
  digitalWrite(enablePIN, LOW);
}

/*  code to process time sync messages from the serial port   */
#define TIME_HEADER  "T"   // Header tag for serial time sync message

unsigned long processSyncMessage () {
  unsigned long pctime = 0L;
  const unsigned long DEFAULT_TIME = 1357041600; // Jan 1 2013 

  if (Serial.find(TIME_HEADER)) {
     pctime = Serial.parseInt();
     return pctime;
     if (pctime < DEFAULT_TIME) { // check the value is a valid time (greater than Jan 1 2013)
       pctime = 0L; // return 0 to indicate that the time is not valid
     }
  }
  return pctime;
}

