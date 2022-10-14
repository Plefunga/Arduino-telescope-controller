#include <AccelStepper.h>

int iY, iM, iD, ih, im, is;

int joyrapin = A0;
int joydecpin = A1;

int joyrainit;
int joydecinit;

int prevjoyra, prevjoydec;

int joyra, joydec;

elapsedMillis tdif;

//due to the reduced precision of doubles at big numbers, I need to 

double LST;
double longitude = 150.944799;

double jdify(int Y, int M, int D, int h, int m, int s){
  int A = floor(Y/100);
  int B = floor(A/4);
  int C = floor(2-A+B);
  int E = floor(365.25*(Y+4716));
  int F = floor(30.6001*(M+1));
  double JD = double(C) + double(D) + double(E) + double(F) -1524.5 + double(ih)/24.0 + double(im)/1440.0 + double(is)/86400.0;
  return JD;
}

double bigt(double jd){
  return ((jd - 2451545.0) / 36525.0);
}

double gmst(double t, double jd){
  double thetazero =  280.46061837 + 360.98564736629 * (jd - 2451545.0) + 0.000387933 * (t*t) - (t*t*t) / 38710000.0;
  while(thetazero > 360){
    thetazero -= 360.0;
  }
  while(thetazero < 0){
    thetazero += 360.0;
  }
  return thetazero;
}

/*  Instead of parallel, do asynchronious analogue read i.e. wait a bunch of cycles before the next analogue read*/

const double rasiderealrate = 100;
const double decsiderealrate = 0;

const double decgear = 2000;
const double ragear = 3200/1.25;

bool moving = false;

char c;
String serial;

char empty;
String blank;

double dec = 90.0*decgear;
double ra = 0;
double todec, tora;
int SPD;
String dir = "0000";
String ins = "000";
bool listening = true;
bool joystick = false;

int maxspeed = 100000;

AccelStepper ramotor(1,2,3);
AccelStepper decmotor(1,4,5);

void setup() {
  Serial1.begin(115200);
  Serial.begin(99999999);
  decmotor.setCurrentPosition(90.0*decgear);
  ramotor.setAcceleration(400);
  ramotor.setMaxSpeed(4000);
  decmotor.setAcceleration(400);
  decmotor.setMaxSpeed(4000);
  tora = ramotor.currentPosition();
  ra = ramotor.currentPosition();
  todec = decmotor.currentPosition();
  dec = decmotor.currentPosition();
  pinMode(LED_BUILTIN, OUTPUT);
  joyrainit = analogRead(joyrapin);
  joydecinit = analogRead(joydecpin);
}

void loop() {  
  if((moving == false) and (joystick = false)){
    ramotor.runSpeed();
    decmotor.runSpeed();
  }
  if(moving == true){
    joyra = 0;
    joydec = 0;
    decmotor.run();
    ramotor.run();
  }
  if((moving == false) and (joystick == true)){
    ramotor.runSpeed();
    decmotor.runSpeed();
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
  }

  
  
  if(listening){
    if(Serial1.available()) {
      c = Serial1.read();
      serial = serial + String(c);
      
      if(c == '%'){
        serial = "";
        char cc = empty;
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
        
        if (iM <= 2){
          iM = iM + 12;
          iY = iY - 1;
        }
        LST = gmst(bigt(jdify(iY, iM, iD, ih, im, is)), jdify(iY, iM, iD, ih, im, is)) + longitude;
        tdif = 0;
        Serial.println(String(iY)+'\t'+String(iM)+'\t'+String(iD)+'\t'+String(ih)+'\t'+String(im)+'\t'+String(is));
        digitalWrite(LED_BUILTIN, HIGH);
        delay(10);
        digitalWrite(LED_BUILTIN, LOW);
      }
      
      if(serial.endsWith("$R|")){
        char cc = empty;
        serial = "";
        while(cc != '|'){
          ramotor.setSpeed(rasiderealrate);
          decmotor.setSpeed(decsiderealrate);
          ramotor.runSpeed();
          decmotor.runSpeed();
          if(Serial1.available() > 0){
            cc = Serial1.read();
            serial = serial + cc;
          }
        }
        if(ih){
          is += tdif / 1000;
        }
        tdif = 0;
        LST = gmst(bigt(jdify(iY, iM, iD, ih, im, is)), jdify(iY, iM, iD, ih, im, is)) + longitude;
        Serial.println(LST);
        tora = double(serial.toFloat());
        tora = LST - tora;
        if(tora < -0){
          tora = tora + 360;
        }
        tora = double(tora) * ragear;
        serial = "";
      }
      
      if(serial.endsWith("$D|")){
        char cc = empty;
        serial = "";
        while(cc != '|'){
          ramotor.setSpeed(rasiderealrate);
          decmotor.setSpeed(decsiderealrate);
          ramotor.runSpeed();
          decmotor.runSpeed();
          if(Serial1.available() > 0){
            cc = Serial1.read();
            serial = serial + cc;
          }
        }
        todec = double(serial.toFloat());
        todec = double(todec) * decgear;
        serial = "";
      }
      
      if(serial.endsWith("$M")){
        serial = "";
        moving = true;
        ramotor.stop();
        decmotor.stop();
        ramotor.moveTo(tora);
        decmotor.moveTo(todec);
        //Serial.println("|"+String(tora)+'\t'+String(todec)+"|");
        digitalWrite(LED_BUILTIN, HIGH);
        if((abs(dec-todec)> 0.001)and(abs(ra-tora)> 0.001)){
          listening = false;
        }
      }
    }
  }

  /* Updators */
  dec = decmotor.currentPosition();
  ra = ramotor.currentPosition();
  //if((abs(dec-todec)< 0.001)and(abs(ra-tora)< 0.001)){ //check whether using 0.toDouble or (double)0 will work
  if((int(dec-todec) == 0) and (int(ra-tora) == 0)){
    moving = false;
    //Serial.println("stopped");
    digitalWrite(LED_BUILTIN, LOW);
    tora = 0;
    todec = 0;
    listening = true;
    ramotor.setSpeed(rasiderealrate);
    decmotor.setSpeed(decsiderealrate);
  }
  /*if(millis() % 1000 == 0){  causing weird problem that limits speed.
    decmotor.run();
    ramotor.run();
    is += 1;
    decmotor.run();
    ramotor.run();
    LST = gmst(bigt(jdify(iY, iM, iD, ih, im, is)), jdify(iY, iM, iD, ih, im, is)) + longitude;
    decmotor.run();
    ramotor.run();
    //Serial.println("Debug Acrux (Ra 186.6496 Dec -63.0991): HA: "+String(LST-186.6496));
    //Serial.println(String(ra)+'\t' + String(dec) + '\t' + String(tora)+'\t' + String(todec)+'\t'+String(ih)+":"+String(im)+":"+String(is));
  }*/
}
