#include <LiquidCrystal_PCF8574.h>
#include <Wire.h>
#include <Angle.h> 

LiquidCrystal_PCF8574 lcd(0x27);

String serial;
char c;
bool precision = false;

Angle ra(0, 0, 0, 0);
Angle dec(90, 0, 0, 0);

String iY, iM, iD, ih, im, is;

String strdec;

String datetime;


void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  
  int e;
  Wire.begin();
  Wire.beginTransmission(0x27);
  e = Wire.endTransmission();
  if (e == 0){
    lcd.begin(20, 4);
  }
  else{
    while(true){
      digitalWrite(LED_BUILTIN, HIGH);
      delay(500);
      digitalWrite(LED_BUILTIN, LOW);
      delay(500);
    }
  }
  lcd.begin(20, 4);
  lcd.setCursor(0, 0);
  lcd.setBacklight(255);
  lcd.print("Time:");
  Serial.begin(9600);
  Serial3.begin(9600);
  Serial1.begin(115200);
  digitalWrite(LED_BUILTIN, HIGH);
  char ccc = "";
  while(ccc != '@'){
    if(Serial3.available()){
      ccc = Serial3.read();
      if(ccc == '!'){
        while(ccc != '&'){
          if(Serial3.available()){
            ccc = Serial3.read();
            if(ccc != '&'){
              datetime += String(ccc);
            }
          }
        }
      }
    }
  }
  Serial1.print(datetime);
  Serial.println(datetime);
  String datetimelcd = datetime.substring(1, 15);
  Serial.println(datetimelcd);
  lcd.setCursor(0, 1);
  iY = datetimelcd.substring(0, 4);
  iM = datetimelcd.substring(4, 6);
  iD = datetimelcd.substring(6, 8);
  ih = datetimelcd.substring(8, 10);
  im = datetimelcd.substring(10, 12);
  is = datetimelcd.substring(12);
  lcd.print(ih + ":" + im + ":" + is + " " + iD + "/" + iM + "/" + iY);
  digitalWrite(LED_BUILTIN, LOW);
}

void loop() {
  
  if(Serial.available()){
    c = Serial.read();
    serial += String(c);
    
    if(serial.endsWith("#:GR#") or serial == "#:GR#"){//Get Right Ascention
      String rahstr;
      String ramstr;
      String rasstr;
      float f;
      int rah, ram, ras;
      f = ra.degree();
      f = f/15;
      rah = f;
      ram=ra.minute();
      ras = ra.second();
      if(rah <= 9){
        rahstr = "0" + String(rah);
      }
      else{
        rahstr = String(rah);
      }
      if(ram <= 9){
        ramstr = "0" + String(ram);
      }
      else{
        ramstr = String(ram);
      }
      if(ras <= 9){
        rasstr = "0" + String(ras);
      }
      else{
        rasstr = String(ras);
      }
      Serial.print("#" + rahstr + ":" + ramstr + ":00#");
      serial = "";
    }
    
    if(serial == "#:GD#"){//Get Declination
      String decmin;
      String decsec;
      String decdeg;
      int deg = dec.degree()-90;
      if(abs(deg) <= 9){
        if(deg <= 9 && deg >= 0){
          decdeg = "+0" + String(abs(deg));
        }
        if(deg >= -9 && deg < 0){
          decdeg = "-0" + String(abs(deg));
        }
      }
      else{
        if(deg > 0){
          decdeg = "+" + String(deg);
        }
        if(deg < 0){
          decdeg = String(deg);
        }
      }
      if(dec.minute() <= 9){
        decmin = "0" + String(dec.minute());
      }
      else{
        decmin = String(dec.minute());
      }
      if(dec.second() <= 9){
        decsec = "0" + String(dec.second());
      }
      else{
        decsec = String(dec.second());
      }
      Serial.print("#" +decdeg + "*"+decmin+":"+decsec+"#");
      serial = "";
    }

    
    if(serial == "#:U#"){//High precision vs low precision. For most of this I don't care about it.
      precision = !precision;
      serial = "";
    }
    
    if(serial == "#:Q#"){//Stop.
      serial = "";
    }

    if(serial == ":Sr"){ //set ra
      char cc;
      while(cc != '#'){
        if(Serial.available() > 0){
          cc = Serial.read();
          serial += String(cc);
        }
      }
      serial = serial.substring(3);
      ra = Angle((serial.substring(0, 2).toInt()*15), serial.substring(3, 5).toInt(), serial.substring(6,8).toInt(), 0);
      serial = "";
      Serial.print("1");
      delay(500);
    }

    if(serial.endsWith(":Sd")){ //set dec
      char cc;
      int todec;
      while(cc != '#'){
        if(Serial.available() > 0){
          cc = Serial.read();
          serial += String(cc);
        }
      }
      if(serial.charAt(3) == '-'){
        todec = (serial.substring(3,6)).toInt();
        todec = todec +90;
      }
      if(serial.charAt(3) == '+'){
        todec = (serial.substring(4,6)).toInt();
        todec = todec + 90;
      }
      dec = Angle(todec);
      Serial.print("1");
      serial = "";
    }

    if(serial.endsWith(":MS#")){ //move
      Serial.print("0");
      Serial1.print("$R|"+String(ra.toDouble())+"|");
      Serial1.print("$D|"+String(dec.toDouble()-90)+"|");
      Serial1.print("$M");
      serial = "";
    }
  }
  delay(1);
}
