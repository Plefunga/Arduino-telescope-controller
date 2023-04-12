/*
  Copyright (C) 2023 Nathan Carter

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  To read the full terms and conditions, see https://www.gnu.org/licenses/.
*/

#include <AccelStepper.h>
#include <AstroCalcs.h>

// RTC stuff. Change this for your RTC implementation. Mine is found here:
// https://www.instructables.com/Interfacing-DS1307-I2C-RTC-With-Arduino/
#include <Wire.h>
#include <RTClib.h>
RTC_DS1307 rtc;

// constants
#define KING_RATE 0.004178 // degrees per second
#define YEAR_SEGMENT 0, 4
#define MONTH_SEGMENT 4, 6
#define DAY_SEGMENT 6, 8
#define HOUR_SEGMENT 8, 10
#define MINUTE_SEGMENT 10, 12
#define SECOND_SEGMENT 12

// for debugging -- prints all debug messages to the COMPUTER_DEBUG_SERIAL
#define DEBUG true
#define DEBUG_PRINT_MOTOR_POSITIONS 1 // uncomment if no motors attached

// location -- change this for your site
#define LONGITUDE 150.944799
#define LATITUDE 31.078821

// general hardware stuff -- change to suit your needs
#define STELLARIUM_INTERFACE_SERIAL Serial1
#define COMPUTER_DEBUG_SERIAL Serial

#define JOYSTICK_RA_PIN  A0
#define JOYSTICK_DEC_PIN A1

#define JOYSTICK_THRESHOLD 20
#define JOYSTICK_SENSITIVITY 10
#define JOYSTICK_INCREMENTS 10

#define DEC_GEAR_RATIO 2000 // in steps per degree
#define RA_GEAR_RATIO 2560 // in steps per degree


// intitialise Astrocalcs
AstroCalcs calcs(LONGITUDE, LATITUDE);

/* joystick */
int joyrainit = 0;
int joydecinit = 0;
int prevjoyra = 0;
int prevjoydec = 0;
int joyra = 0;
int joydec = 0;

/*stuff that matters when moving*/
double rasiderealrate = KING_RATE*RA_GEAR_RATIO; //0.004178 degrees per second * gear ratio
double decsiderealrate = 0; // will implement refractive correction

bool moving = false;
bool parking = false;
bool align = false;
double alignra = 0.0;
double aligndec = 0.0;

/* Serial variables */
char c;
String serial = "";
char emptychar;
String blank = "";

/* intital conditions */
double dec = 0.0;
double ra = 0.0;
double ramotorpos = 0.0; //ha*RA_GEAR_RATIO
double decmotorpos = dec*DEC_GEAR_RATIO;
double todec = 0.0;
double tora = 0.0;
double toha = 0.0;
double todecmotorpos = 0.0;
double toramotorpos = 0.0;
bool listening = true;
bool joystick = false;

/*
 * as a note: ramotorpos and toramotorpos are not equal to right ascention in any way! they are absolute motor positions, whereas todec, tora, ra and dec are actual ra and dec positions (independant of time)!
 * the reason they are coalled that are because they are for the ra motor -- a motor which controls the RA axis.
 */

AccelStepper ramotor(1,2,3);
AccelStepper decmotor(1,4,5);

void setup()
{
  // Turn on LED when initialising
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  // Initialise Serial
  STELLARIUM_INTERFACE_SERIAL.begin(115200); 
  COMPUTER_DEBUG_SERIAL.begin(99999999); // an arbitrarily fast speed, as the teensy runs at full USB speed anyway
  Info("Serial Initialised");
  if(DEBUG){
    Info("Debug set to true. All debug messages will be sent to Serial (unless slewing at high speeds");
  }

  // calibrate Joysticks
  joyrainit = analogRead(JOYSTICK_RA_PIN);
  joydecinit = analogRead(JOYSTICK_DEC_PIN);
  Info("Joystick calibrated");

  // set motor speeds, acceleration rates and max speeds
  ramotor.setSpeed(rasiderealrate);
  decmotor.setSpeed(decsiderealrate);
  ramotor.setAcceleration(400);
  ramotor.setMaxSpeed(3000);
  decmotor.setAcceleration(400);
  decmotor.setMaxSpeed(3000);

  // Initialise Wire and RTC
  Wire.begin();
  if(!rtc.begin()){
    Error("RTC either not detected or failed. Check RTC");
    
    // flash builtin led for 2 seconds
    for(int i = 0; i < 2; i++)
    {
      digitalWrite(LED_BUILTIN, LOW);
      delay(100);
      digitalWrite(LED_BUILTIN, HIGH);
      delay(100);
    }
  }
  // get RTC time
  DateTime rtctime = rtc.now();

  Info("RTC initialised");

  // set time to RTC time
  int iY = rtctime.year();
  int iM = rtctime.month();
  int iD = rtctime.day();
  int ih = rtctime.hour();
  int im = rtctime.minute();
  int is = rtctime.second();

  Info("Updating time...");
  calcs.updateTime(iY, iM, iD, ih, im, is);
  Info("Time updated: LST is " + String(calcs.getLST()));
  Info("Time is: "+String(ih)+":"+String(im)+":"+String(is) +" " + String(iD)+"/"+String(iM)+"/"+String(iY));
  
  // calculate the ra and dec of the park position
  Info("Calculating inital position...");
  calcs.setAltAz(0, 90);

  ra = calcs.getRA();
  dec = calcs.getDec();

  // calculate the gear position of the ra and dec
  ramotorpos = calcs.getHA()*RA_GEAR_RATIO;
  decmotorpos = dec*DEC_GEAR_RATIO;

  // set the current position of the motors to this
  decmotor.setCurrentPosition(decmotorpos);
  ramotor.setCurrentPosition(ramotorpos);

  // set motor position variables
  ramotorpos = ramotor.currentPosition();
  toramotorpos = ramotor.currentPosition();
  decmotorpos = decmotor.currentPosition();
  todecmotorpos = decmotor.currentPosition();

  Info("Initial position calculated:");
  Info("RA: " + String(ra));
  Info("DEC: " + String(dec));

  // finish setup, turn off builtin led
  digitalWrite(LED_BUILTIN, LOW);
}

void loop()
{
  if(moving)
  {
    // clear any joystick variables
    joyra = 0;
    joydec = 0;
    // tell the motors to keep on running
    decmotor.run();
    ramotor.run();
  }
  
  if(!moving)
  {
    // check only once every millisecond as I do not want to incorporate a
    // delay into the loop, but analogue read needs a cooldown
    if(millis() % 1 == 0)
    {
      // calculate the speed to run the motors at based on joystick input
      // factor calibration, average with previous value
      // then divide and multiply by JOYSTICK_INCREMENTS to make it work only in increments
      // before I did this, the speed kept on changing due to analogue noise, which
      // made a tremendous noise as the motors kept on rapidly changing
      joyra = (analogRead(JOYSTICK_RA_PIN) - joyrainit + prevjoyra)/(2*JOYSTICK_INCREMENTS);
      joydec = (analogRead(JOYSTICK_DEC_PIN) - joydecinit + prevjoydec)/(2*JOYSTICK_INCREMENTS);
      joyra *= JOYSTICK_INCREMENTS;
      joydec *= JOYSTICK_INCREMENTS;

      // if greater than JOYSTICK_THRESHOLD, then move
      if((abs(joyra)>=JOYSTICK_THRESHOLD)or(abs(joydec)>=JOYSTICK_THRESHOLD))
      {
        // set speed
        ramotor.setSpeed((int)(joyra*JOYSTICK_SENSITIVITY));
        decmotor.setSpeed((int)(joydec*JOYSTICK_SENSITIVITY));
        
        // poll motors to run
        ramotor.runSpeed();
        decmotor.runSpeed();

        // tell the rest of the program that they joystick is being used
        joystick = true;
      }

      else
      {
        // joystick has stopped being used
        joystick = false;

        // return to normal speed
        ramotor.setSpeed(rasiderealrate);
        decmotor.setSpeed(decsiderealrate);
      }
    }

    // always run at some speed -- whether sidereal or the joystick speed
    ramotor.runSpeed();
    decmotor.runSpeed();
  }

  
  /******   SERIAL COMMUNICATION   ******/
  // if it is not moving quickly
  if(listening)
  {
    // if serial interface is there, then actually use it
    if(STELLARIUM_INTERFACE_SERIAL.available())
    {
      // todo -- find out why this is here
      delay(100);

      // read serial
      c = STELLARIUM_INTERFACE_SERIAL.read();
      serial = serial + String(c);
      
      // update time command -- see docs about what this command looks like
      if(c == '%')
      {
        // read serial
        serial = "";
        char cc = emptychar;
        String s = blank;
        Info("updating time...");
        while((STELLARIUM_INTERFACE_SERIAL.available()) and (cc != '^'))
        {
          cc = STELLARIUM_INTERFACE_SERIAL.read();
          // if command not finished, keep on reading
          if(cc != '&'){
            s = s + String(cc);
          }
        }

        // trimmed message looks like this: (see preprocessor directives and docs)
        //   Y Y Y Y M M D D h h m  m  s  s
        //   0 1 2 3 4 5 6 7 8 9 10 11 12 13
        int iY = s.substring(YEAR_SEGMENT).toInt();
        int iM = s.substring(MONTH_SEGMENT).toInt();
        int iD = s.substring(DAY_SEGMENT).toInt();
        int ih = s.substring(HOUR_SEGMENT).toInt();
        int im = s.substring(MINUTE_SEGMENT).toInt();
        int is = s.substring(SECOND_SEGMENT).toInt();

        // adjust RTC
        rtc.adjust(DateTime(iY, iM, iD, ih, im, is));

        Info("Time recieved: "+String(ih)+":"+String(im)+":"+String(is) +" " + String(iD)+"/"+String(iM)+"/"+String(iY));
        Info("Updating time...");

        // update AstroCalcs time
        calcs.updateTime(iY, iM, iD, ih, im, is);

        Info("Updated time. LST: "+String(calcs.getLST()));

        // flash LED to show that time was recieved
        digitalWrite(LED_BUILTIN, HIGH);
        delay(100);
        digitalWrite(LED_BUILTIN, LOW);

        // tell the Stellarium Interface board the variables that AstroCalcs calculates,
        // as it takes too long to do them itself
        STELLARIUM_INTERFACE_SERIAL.println(calcs.timeVars()+"$");
        Info("Updated time for SI with: "+calcs.timeVars());
      }
      
      // Set target RA -- see docs about what this command looks like
      if(serial.endsWith("$R|"))
      {
        // clear serial variables for reuse
        char cc = emptychar;
        serial = "";
        while(cc != '|')
        {
          // run motors still
          ramotor.runSpeed();
          decmotor.runSpeed();

          // read serial
          if(STELLARIUM_INTERFACE_SERIAL.available() > 0)
          {
            cc = STELLARIUM_INTERFACE_SERIAL.read();
            serial = serial + cc;
          }
        }

        // update time
        updateRTCtime(rtc.now());

        // set target position
        tora = double(serial.toFloat());

        // clear serial variable for reuse
        serial = "";

        Info("RA position recieved: "+String(tora));
      }
      
      // set declination command -- see docs for description
      if(serial.endsWith("$D|"))
      {
        // clear serial variables for reuse
        char cc = emptychar;
        serial = "";

        // read serial until end of command
        while(cc != '|')
        {
          // run motors
          ramotor.runSpeed();
          decmotor.runSpeed();

          // read serial if it is available
          if(STELLARIUM_INTERFACE_SERIAL.available() > 0)
          {
            cc = STELLARIUM_INTERFACE_SERIAL.read();
            serial = serial + cc;
          }
        }

        // set target declination
        todec = double(serial.toFloat());

        // clear serial variables
        serial = "";

        Info("Dec position recieved: "+String(todec));
      }
      
      // Move command -- see docs for full description
      if(serial.endsWith("$M"))
      {
        // clear serial variables for reuse
        serial = "";
        
        // we are now going to be moving at high speeds, 
        // so tell the rest of the program that we are moving
        moving = true;

        // stop the motors from running at constant speed
        ramotor.stop();
        decmotor.stop();

        // update the time
        updateRTCtime(rtc.now());

        // set coordinates
        calcs.setRADEC(tora,todec);

        // correct for precession, refraction, etc. to get JNOW
        calcs.calcPosJ2000(tora, todec);

        // get actual positions
        toha = calcs.getHA();
        todec = calcs.getDec();

        // correct for any accidental range mishaps
        if(toha < -0){
          toha = toha + 360;
        }
        if(abs((calcs.getLST()-ra) - toha) > abs((calcs.getLST()-ra) - (toha-360))){
          toha = toha - 360;
        }

        // set gear positions
        toramotorpos = toha * RA_GEAR_RATIO;
        todecmotorpos = todec * DEC_GEAR_RATIO;
        
        // move to gear positions
        ramotor.moveTo(toramotorpos);
        decmotor.moveTo(todecmotorpos);

        // turn on the LED to show that we are on the move
        digitalWrite(LED_BUILTIN, HIGH);

        // stop listening on Serial
        listening = false;

        // if we are aligning, don't get rid of the target coordinates
        if(align == true)
        {
          alignra = tora;
          aligndec = todec;
        }

        Info("Move command recieved");
      }

      // park command -- read docs for full description
      if(serial.endsWith("$P"))
      {
        // update time
        updateRTCtime(rtc.now());

        Info("Park command recieved. Parking...");

        // clear serial variable for reuse
        serial = "";

        // tell the rest of the program that we are moving
        moving = true;

        // stop the motors
        ramotor.stop();
        decmotor.stop();

        // set the position to the park position
        Info("Setting position...");
        calcs.setAltAz(0, 90);

        // get the ra and dec of that park position (no need to correct for refraction)
        ra = calcs.getRA();
        dec = calcs.getDec();
        toha = calcs.getHA();

        // correct for any range mishaps
        if(toha < -0.0){
          toha = toha + 360;
        }

        // calculate the target gear positions
        toramotorpos = toha * RA_GEAR_RATIO;
        todecmotorpos = todec * DEC_GEAR_RATIO;

        // slew
        Info("Set Position. Slewing...");
        ramotor.moveTo(toramotorpos);
        decmotor.moveTo(todecmotorpos);

        // turn on the light to show that it is slewing
        digitalWrite(LED_BUILTIN, HIGH);

        // stop serial communication
        listening = false;

        // tell the program that we are going to park
        parking = true;
      }
      
      // align command -- see docs for full description
      if(serial.endsWith("$A"))
      {
        // clear serial variable for reuse
        serial = "";

        // if we were already doing alignment
        if(align == true)
        {
          // update time
          updateRTCtime(rtc.now());

          // set the current position to the position it should be at
          ramotor.setCurrentPosition((calcs.getLST()-alignra)*RA_GEAR_RATIO);
          decmotor.setCurrentPosition(aligndec*DEC_GEAR_RATIO);

          // update the motor position variables
          decmotorpos = decmotor.currentPosition();
          ramotorpos = ramotor.currentPosition();

          // update the ra and dec variables
          dec = decmotorpos/DEC_GEAR_RATIO;
          ra = calcs.getLST() - ramotorpos/RA_GEAR_RATIO;

          // we are no longer aligning
          align = false;
          Info("Align command recieved. Position calibrated.");
        }

        else
        {
          // next time this command is recieved, we are actually setting the position
          align = true;
          Info("Align command recieved. Waiting for positions...");
        }
      }
    }
  }

  /******   THINGS TO UPDATE   ******/
  decmotorpos = decmotor.currentPosition();
  ramotorpos = ramotor.currentPosition();

  dec = decmotorpos/DEC_GEAR_RATIO;
  ra = calcs.getLST() - ramotorpos/RA_GEAR_RATIO;

  // as floats don't equate consitently, convert to int and then compare
  // ramotorpos and decmotorpos are very large as they are actual motor
  // step positions, and so no precision is lost by converting to int
  if((int(decmotorpos-todecmotorpos) == 0) and (int(ramotorpos-toramotorpos) == 0))
  {
    // no longer moving
    moving = false;

    Info("Finished slewing");
    digitalWrite(LED_BUILTIN, LOW);

    // clear target variables
    tora = 0;
    todec = 0;
    toha = 0;
    toramotorpos = 0;
    todecmotorpos = 0;

    // start listening on serial again
    listening = true;

    // if parking
    if(parking)
    {
      Info("Parked. Turn off power.");
      
      // do nothing
      while(true){}
    }
    // set speed to sidereal rate
    ramotor.setSpeed(rasiderealrate);
    decmotor.setSpeed(decsiderealrate);
  }

  /******   DEBUG PRINT MOTOR POSITION   ******/
  #ifdef DEBUG_PRINT_MOTOR_POSITIONS
  // run once per second
  if(millis() % 1000 == 0)
  {
    // update time
    updateRTCtime(rtc.now());
    
    // get time vars
    DateTime rtctime = rtc.now();
    int ih = rtctime.hour();
    int im = rtctime.minute();
    int is = rtctime.second();
    // print motor positions -- change this how you like it
    COMPUTER_DEBUG_SERIAL.println(String(ra)+'\t' + String(dec) + '\t' + String(tora)+'\t' + String(todec)+'\t'+String(ih)+":"+String(im)+":"+String(is) +'\t'+String(ramotorpos)+" "+String(decmotorpos));
    delay(1);
  }
  #endif
}

/******   UPDATE TIME FUNCTION   ******/
void updateRTCtime(DateTime rtctime)
{
  // get time
  int iY = rtctime.year();
  int iM = rtctime.month();
  int iD = rtctime.day();
  int ih = rtctime.hour();
  int im = rtctime.minute();
  int is = rtctime.second();
  // set time
  calcs.updateTime(iY, iM, iD, ih, im, is);
}

/******   DEBUG PRINT FUNCTIONS   ******/
void Info(String s)
{
  if(DEBUG){
    Serial.println("[INFO] "+s);
  }
}

void Error(String s)
{
  if(DEBUG){
    COMPUTER_DEBUG_SERIAL.println("[ERROR] "+s);
  }
}
void Warn(String s)
{
  if(DEBUG){
    COMPUTER_DEBUG_SERIAL.println("[WARN] "+s);
  }
}
