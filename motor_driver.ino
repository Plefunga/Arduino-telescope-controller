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
#include <Wire.h>
#include <RTClib.h>
//https://www.instructables.com/Interfacing-DS1307-I2C-RTC-With-Arduino/
RTC_DS1307 rtc;

/*location and time*/
double longitude = 150.944799;
double latitude = 31.078821;
int iY, iM, iD, ih, im, is;
AstroCalcs calcs(longitude, latitude);
elapsedMillis tdif;

/*joystick*/
int joyrapin = A0;
int joydecpin = A1;
int joyrainit;
int joydecinit;
int prevjoyra, prevjoydec;
int joyra, joydec;

/*stuff that matters when moving*/
const double decgear = 2000; //steps per degree
const double ragear = 3200/1.25; //same as above

double rasiderealrate = 0.004178*ragear; //0.004178 degrees per second * gear ratio
double decsiderealrate = 0;

bool moving = false;
bool parking = false;
bool align = false;
double alignra, aligndec;

/*Serial variables*/
char c;
String serial;
char emptychar;
String blank;

bool debug = true;

/*intital conditions*/
double dec = 0;
double ra = 0;
double ramotorpos = 0; //ha*ragear
double decmotorpos = dec*decgear;
double todec, tora, toha, todecmotorpos, toramotorpos;
bool listening = true;
bool joystick = false;

/*
 * as a note: ramotorpos and toramotorpos are not equal to right ascention in any way! they are absolute motor positions, whereas todec, tora, ra and dec are actual ra and dec positions (independant of time)!
 */

AccelStepper ramotor(1,2,3);
AccelStepper decmotor(1,4,5);

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(500);
  digitalWrite(LED_BUILTIN, LOW);
  Serial1.begin(115200); 
  Serial.begin(99999999);
  Info("Serial Initialised");
  if(debug){
    Info("Debug set to true. All debug messages will be sent to Serial (unless slewing at high speeds");
  }
  Wire.begin();
  if(!rtc.begin()){
    Error("RTC either not detected or failed. Check RTC");
  }
  DateTime rtctime = rtc.now();
  Info("RTC initialised");
  joyrainit = analogRead(joyrapin);
  joydecinit = analogRead(joydecpin);
  Info("Joystick calibrated");
  ramotor.setSpeed(rasiderealrate);
  decmotor.setSpeed(decsiderealrate);
  iY = rtctime.year();
  iM = rtctime.month();
  iD = rtctime.day();
  ih = rtctime.hour();
  im = rtctime.minute();
  is = rtctime.second();
  /*String s = "20230126101923";
  iY = s.substring(0, 4).toInt();
  iM = s.substring(4, 6).toInt();
  iD = s.substring(6, 8).toInt();
  ih = s.substring(8, 10).toInt();
  im = s.substring(10, 12).toInt();
  is = s.substring(12).toInt();*/
  //rtc.adjust(DateTime(iY, iM, iD, ih, im, (int)is));
  Info("Updating time...");
  calcs.updateTime(iY, iM, iD, ih, im, is);
  Info("Time updated: LST is " + String(calcs.getLST()));
  Info("Time is: "+String(ih)+":"+String(im)+":"+String(is) +" " + String(iD)+"/"+String(iM)+"/"+String(iY));
  Info("Calculating inital position...");
  calcs.setAltAz(0, 90);
  ra = calcs.getRA();
  dec = calcs.getDec();
  ramotorpos = calcs.getHA()*ragear;
  decmotorpos = dec*decgear;
  decmotor.setCurrentPosition(decmotorpos);
  ramotor.setCurrentPosition(ramotorpos);
  ramotor.setAcceleration(400);
  ramotor.setMaxSpeed(3000);
  decmotor.setAcceleration(400);
  decmotor.setMaxSpeed(3000);
  ramotorpos = ramotor.currentPosition();
  toramotorpos = ramotor.currentPosition();
  decmotorpos = decmotor.currentPosition();
  todecmotorpos = decmotor.currentPosition();
  Info("Initial position calculated:");
  Info(String(ra));
  Info(String(dec));
}

void loop() {  
  if(moving == true){
    joyra = 0;
    joydec = 0;
    decmotor.run();
    ramotor.run();
  }
  
  if(moving == false){
    if((millis())%1== 0){
      joyra = (analogRead(joyrapin) - joyrainit + prevjoyra)/20;
      joydec = (analogRead(joydecpin) - joydecinit + prevjoydec)/20;
      joyra *= 10;
      joydec *= 10;
      if((abs(joyra)>=20)or(abs(joydec)>=20)){
        //Serial.println(String((int)(joyra))+'\t'+String((int)(joydec)));
        ramotor.setSpeed((int)(joyra*10));
        decmotor.setSpeed((int)(joydec*10));
        ramotor.runSpeed();
        decmotor.runSpeed();
        joystick = true;
      }
      else{
        joystick = false;
        ramotor.setSpeed(rasiderealrate);
        decmotor.setSpeed(decsiderealrate);
      }
    }
    ramotor.runSpeed();
    decmotor.runSpeed();
  }

  
  
  if(listening){
    if(Serial1.available()) {
      delay(100);
      c = Serial1.read();
      serial = serial + String(c);
      
      if(c == '%'){
        serial = "";
        char cc = emptychar;
        String s = blank;
        while((Serial1.available()) and (cc != '^')){
          cc = Serial1.read();
          if(cc != '&'){
            s = s + String(cc);
          }
        }
        Serial.println(s);
        //   Y Y Y Y M M D D h h m  m  s  s
        //   0 1 2 3 4 5 6 7 8 9 10 11 12 13
        iY = s.substring(0, 4).toInt();
        iM = s.substring(4, 6).toInt();
        iD = s.substring(6, 8).toInt();
        ih = s.substring(8, 10).toInt();
        im = s.substring(10, 12).toInt();
        is = s.substring(12).toInt();
        rtc.adjust(DateTime(iY, iM, iD, ih, im, (int)is));
        Info("Time recieved: "+String(ih)+":"+String(im)+":"+String(is) +" " + String(iD)+"/"+String(iM)+"/"+String(iY));
        Info("Updating time...");
        calcs.updateTime(iY, iM, iD, ih, im, is);
        tdif = 0;
        Info("Updated time. LST: "+String(calcs.getLST()));
        digitalWrite(LED_BUILTIN, HIGH);
        delay(100);
        digitalWrite(LED_BUILTIN, LOW);
        Serial1.println(calcs.timeVars()+"$");
        Info("Updated time for SI with: "+calcs.timeVars());
      }
      
      if(serial.endsWith("$R|")){
        char cc = emptychar;
        serial = "";
        while(cc != '|'){
          ramotor.runSpeed();
          decmotor.runSpeed();
          if(Serial1.available() > 0){
            cc = Serial1.read();
            serial = serial + cc;
          }
        }
        //updateRTCtime();
        //while I haven't soldered the headers onto the RTC:
        is += tdif / 1000;
        tdif = 0;
        
        calcs.updateTime(iY, iM, iD, ih, im, is);
        tora = double(serial.toFloat());
        serial = "";
        Info("RA position recieved: "+String(tora));
      }
      
      if(serial.endsWith("$D|")){
        char cc = emptychar;
        serial = "";
        while(cc != '|'){
          ramotor.runSpeed();
          decmotor.runSpeed();
          if(Serial1.available() > 0){
            cc = Serial1.read();
            serial = serial + cc;
          }
        }
        todec = double(serial.toFloat());
        serial = "";
        Info("Dec position recieved: "+String(todec));
      }
      
      if(serial.endsWith("$M")){
        serial = "";
        moving = true;
        ramotor.stop();
        decmotor.stop();
        //updatetime();
        calcs.setRADEC(tora,todec);
        calcs.calcPosJ2000(tora, todec);
        toha = calcs.getHA();
        todec = calcs.getDec();
        if(toha < -0){
          toha = toha + 360;
        }
        if(abs((calcs.getLST()-ra) - toha) > abs((calcs.getLST()-ra) - (toha-360))){
          toha = toha - 360;
        }
        toramotorpos = toha * ragear;
        todecmotorpos = todec * decgear;
        ramotor.moveTo(toramotorpos);
        decmotor.moveTo(todecmotorpos);
        //Serial.println("|"+String(toha)+'\t'+String(todec)+"|");
        digitalWrite(LED_BUILTIN, HIGH);
        listening = false;
        if(align == true){
          alignra = tora;
          aligndec = todec;
        }
        Info("Move command recieved");
      }
      if(serial.endsWith("$P")){
        //updateRTCtime();
        Info("Park command recieved. Parking...");
        serial = "";
        moving = true;
        ramotor.stop();
        decmotor.stop();
        Info("Setting position...");
        calcs.setAltAz(0, 90);
        ra = calcs.getRA();
        dec = calcs.getDec();
        toha = calcs.getHA();
        if(toha < -0.0){
          toha = toha + 360;
        }
        toramotorpos = toha * ragear;
        todecmotorpos = todec * decgear;
        Info("Set Position. Slewing...");
        ramotor.moveTo(toramotorpos);
        decmotor.moveTo(todecmotorpos);
        //Serial.println("|"+String(toha)+'\t'+String(todec)+"|");
        digitalWrite(LED_BUILTIN, HIGH);
        listening = false;
        parking = true;
      }
      
      if(serial.endsWith("$A")){
        serial = "";
        if(align == true){
          //updateRTCtime();
          ramotor.setCurrentPosition((calcs.getLST()-alignra)*ragear);
          decmotor.setCurrentPosition(aligndec*decgear);
          decmotorpos = decmotor.currentPosition();
          ramotorpos = ramotor.currentPosition();
          dec = decmotorpos/decgear;
          ra = calcs.getLST() - ramotorpos/ragear;
          align = false;
          Info("Align command recieved. Position calibrated.");
        }
        else{
          align = true;
          Info("Align command recieved. Slewing...");
        }
      }
    }
  }

  /* Updators */
  decmotorpos = decmotor.currentPosition();
  ramotorpos = ramotor.currentPosition();
  dec = decmotorpos/decgear;
  ra = calcs.getLST() - ramotorpos/ragear;
  if((int(decmotorpos-todecmotorpos) == 0) and (int(ramotorpos-toramotorpos) == 0)){
    moving = false;
    //Serial.println("stopped");
    Info("Finished slewing");
    digitalWrite(LED_BUILTIN, LOW);
    tora = 0;
    todec = 0;
    toha = 0;
    toramotorpos = 0;
    todecmotorpos = 0;
    listening = true;
    if(parking){
      Info("Parked. Turn off power.");
      while(true){}
    }
    ramotor.setSpeed(rasiderealrate);
    decmotor.setSpeed(decsiderealrate);
  }

  /*Debug and motor position feedback - cant run when motors are actually there*/
  if(millis() % 1000 == 0){
    //updateRTCtime();
    //Serial.println(String(calcs._rah)+":"+String(calcs._ram)+":"+String(calcs._ras)+'\t' + String(dec) + '\t' + String(tora)+'\t' + String(todec)+'\t'+String(ih)+":"+String(im)+":"+String(is) +'\t'+String(ramotorpos)+" "+String(decmotorpos));
    Serial.println(String(ra)+'\t' + String(dec) + '\t' + String(tora)+'\t' + String(todec)+'\t'+String(ih)+":"+String(im)+":"+String(is) +'\t'+String(ramotorpos)+" "+String(decmotorpos));
    delay(1);
  }
}

void Info(String s){
  if(debug){
    Serial.println("[INFO] "+s);
  }
}

void Error(String s){
  if(debug){
    Serial.println("[ERROR] "+s);
  }
}
void Warn(String s){
  if(debug){
    Serial.println("[ERROR] "+s);
  }
}

/*void updateRTCtime(){
  iY = rtctime.year();
  iM = rtctime.month();
  iD = rtctime.day();
  ih = rtctime.hour();
  im = rtctime.minute();
  is = rtctime.second();
  calcs.updateTime(iY, iM, iD, ih, im, is);
}*/