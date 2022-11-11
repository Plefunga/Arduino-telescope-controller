#include <AccelStepper.h>
#include <AstroCalcs.h>

/*location and time*/
double longitude = 150.944799;
double latitude = 31.08;
int iY, iM, iD, ih, im, is;
AstroCalcs calcs(longitude, latitude);
elapsedMillis tdif;
bool timeset = false;

/*joystick*/
int joyrapin = A0;
int joydecpin = A1;
int joyrainit;
int joydecinit;
int prevjoyra, prevjoydec;
int joyra, joydec;

/*stuff that matters when moving*/
double rasiderealrate = 1000;
double decsiderealrate = 110;

const double decgear = 2000;
const double ragear = 3200/1.25;

bool moving = false;


/*Serial variables*/
char c;
String serial;
char emptychar;
String blank;

/*intital conditions*/
double dec = 90.0;
double ra = 0;
double ramotorpos = 0;
double decmotorpos = 90*decgear;
double todec, tora, toha, todecmotorpos, toramotorpos;
bool listening = true;
bool joystick = false;

/*
 * as a note: ramotorpos and toramotorpos are not equal to right ascention in any way! they are absolute motor positions, whereas todec, tora, ra and dec are actual ra and dec positions (independant of time)!
 */

AccelStepper ramotor(1,2,3);
AccelStepper decmotor(1,4,5);

void setup() {
  calcs.setRADEC(ra, dec);
  Serial1.begin(115200);
  Serial.begin(99999999);
  decmotor.setCurrentPosition(90.0*decgear);
  ramotor.setAcceleration(400);
  ramotor.setMaxSpeed(3000);
  decmotor.setAcceleration(400);
  decmotor.setMaxSpeed(3000);
  ramotorpos = ramotor.currentPosition();
  toramotorpos = ramotor.currentPosition();
  decmotorpos = decmotor.currentPosition();
  todecmotorpos = decmotor.currentPosition();
  pinMode(LED_BUILTIN, OUTPUT);
  joyrainit = analogRead(joyrapin);
  joydecinit = analogRead(joydecpin);
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
        //   Y Y Y Y M M D D h h m  m  s  s
        //   0 1 2 3 4 5 6 7 8 9 10 11 12 13
        iY = s.substring(0, 4).toInt();
        iM = s.substring(4, 6).toInt();
        iD = s.substring(6, 8).toInt();
        ih = s.substring(8, 10).toInt();
        im = s.substring(10, 12).toInt();
        is = s.substring(12).toInt();
        calcs.updateTime(iY, iM, iD, ih, im, is);
        tdif = 0;
        Serial.println(String(iY)+'\t'+String(iM)+'\t'+String(iD)+'\t'+String(ih)+'\t'+String(im)+'\t'+String(is));
        digitalWrite(LED_BUILTIN, HIGH);
        delay(10);
        digitalWrite(LED_BUILTIN, LOW);
        timeset = false;
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
        if(timeset){
          is += tdif / 1000;
        }
        tdif = 0;
        calcs.updateTime(iY, iM, iD, ih, im, is);
        tora = double(serial.toFloat());
        serial = "";
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
      }
      
      if(serial.endsWith("$M")){
        serial = "";
        moving = true;
        ramotor.stop();
        decmotor.stop();
        calcs.setRADEC(tora,todec);
        toha = calcs.getHA();
        if(toha < -0){
          toha = toha + 360;
        }
        calcs.refract();
        toramotorpos = toha * ragear;
        todecmotorpos = todec * decgear;
        ramotor.moveTo(toramotorpos);
        decmotor.moveTo(todecmotorpos);
        //Serial.println("|"+String(toha)+'\t'+String(todec)+"|");
        digitalWrite(LED_BUILTIN, HIGH);
        listening = false;
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
    digitalWrite(LED_BUILTIN, LOW);
    tora = 0;
    todec = 0;
    toha = 0;
    toramotorpos = 0;
    todecmotorpos = 0;
    listening = true;
    ramotor.setSpeed(rasiderealrate);
    decmotor.setSpeed(decsiderealrate);
  }
}
