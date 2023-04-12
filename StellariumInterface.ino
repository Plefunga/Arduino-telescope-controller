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


#include <Wire.h>
#include <AstroCalcs.h>

// change the below to suit your specific LCD
#include <LiquidCrystal_PCF8574.h>
LiquidCrystal_PCF8574 lcd(0x27);

// change the below coordinates to your specific location
#define LONGITUDE 150.944799
#define LATITUDE 150.944799
#define STR_LONGITUDE String(150*57)
#define STR_LATITUDE String(-31*05)

AstroCalcs calcs(LONGITUDE, LATITUDE);

// change this according to your mount and hardware
#define SPEED 0.85 // in seconds per degree, for estimating how long a slew will take
#define ALIGN_PIN 7
#define PARK_PIN 6

#define COMPUTER_SERIAL Serial
#define ESP_SERIAL Serial3
#define MOTOR_DRIVER_SERIAL Serial1

#define COMPUTER_SERIAL_BAUDRATE 9600
#define ESP_SERIAL_BAUDRATE 9600
#define MOTOR_DRIVER_SERIAL_BAUDRATE 115200

#define LCD_ADDRESS 0x27

// other preprocessor directives
#define YEAR_SEGMENT 0, 4
#define MONTH_SEGMENT 4, 6
#define DAY_SEGMENT 6, 8
#define HOUR_SEGMENT 8, 10
#define MINUTE_SEGMENT 10, 12
#define SECOND_SEGMENT 12

#define VERSION String(0.4)

// setting various variables
String view = "parked";
String serial = "";
char c;
bool precision = false;
int distance = 0;
int sim = 0;
int estTime = 10; // estimated time of slew

String ack = ":" + String(char(6))+"#"; // used in ASCOM communication

// initialising ra and dec
double ra = 0.0;
double dec = 0.0;

// initial time
String iY, iM, iD, ih, im, is;
String strdec;
String datetime;

bool aligning = false;

void setup()
{
  // initialise serial communication
  COMPUTER_SERIAL.begin(COMPUTER_SERIAL_BAUDRATE);
  ESP_SERIAL.begin(ESP_SERIAL_BAUDRATE);
  MOTOR_DRIVER_SERIAL.begin(MOTOR_DRIVER_SERIAL_BAUDRATE);

  // initialise pinModes for pins
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(ALIGN_PIN, INPUT);
  pinMode(PARK_PIN, INPUT);

  // initialise LCD (and Wire)
  Wire.begin();
  Wire.beginTransmission(LCD_ADDRESS);
  int e = Wire.endTransmission();

  // if LCD exists
  if (e == 0){
    lcd.begin(20, 4);
  }

  else{
    // if no LCD, flash buildin led
    while(true)
    {
      digitalWrite(LED_BUILTIN, HIGH);
      delay(500);
      digitalWrite(LED_BUILTIN, LOW);
      delay(500);
    }
  }

  lcd.begin(20, 4);
  lcd.setBacklight(255);

  // print Aquiring time loading screen
  lcd.setCursor(0, 0);

  /*
  |  ANSSEN ALHENA EQ  |
  |  MOUNT CONTROLLER  |
  |        vA.B        |
  |  Aquiring time...  |
  */

  //         01234567890123456789
  lcd.print("  ANSSEN ALHENA EQ  ");
  lcd.setCursor(0, 1);
  //         01234567890123456789
  lcd.print("  MOUNT CONTROLLER  ");
  lcd.setCursor(0,2);
  //         01234567890123456789
  lcd.print("        v"+VERSION+"        ");
  lcd.setCursor(0,3);
  //         01234567890123456789
  lcd.print("  Aquiring time...  ");

  // turn on LED while loading
  digitalWrite(LED_BUILTIN, HIGH);

  char ccc = "";
  int timeout = millis();

  // request time from ESP
  ESP_SERIAL.print("$GT");

  // wait for time, and if it has been longer than 20 seconds, exit
  while((ccc != '@')and(millis()-timeout<=20000))
  {
    if(ESP_SERIAL.available())
    {
      ccc = ESP_SERIAL.read();
      if(ccc == '!')
      {
        while(ccc != '&')
        {
          if(ESP_SERIAL.available())
          {
            ccc = ESP_SERIAL.read();
            if(ccc != '&'){
              datetime += String(ccc);
            }
          }
        }
      }
    }
  }

  // if it took too long
  if(millis()-timeout > 20000)
  {
    /*
    |  ANSSEN ALHENA EQ  |
    |  MOUNT CONTROLLER  |
    |        vA.B        |
    |  NO INTERNET TIME  |
    */
    lcd.setCursor(0,3);
    //         12345678901234567890
    lcd.print("  NO INTERNET TIME  ");
    delay(1000);
  }

  // tell motor driver board the time
  MOTOR_DRIVER_SERIAL.println(datetime);

  // show initialising screen
  lcd.clear();
  lcd.setCursor(0, 0);
  
  /*
  |  ANSSEN ALHENA EQ  |
  |  MOUNT CONTROLLER  |
  |        vA.B        |
  |  Initialising....  |
  */

  //         01234567890123456789
  lcd.print("  ANSSEN ALHENA EQ  ");
  lcd.setCursor(0, 1);
  //         01234567890123456789
  lcd.print("  MOUNT CONTROLLER  ");
  lcd.setCursor(0,2);
  //         01234567890123456789
  lcd.print("        v"+VERSION+"        ");
  lcd.setCursor(0,3);
  //         01234567890123456789
  lcd.print("  Initialising....  ");

  // extract date and time from string
  String datetimelcd = datetime.substring(1, 15);

  iY = datetimelcd.substring(YEAR_SEGMENT);
  iM = datetimelcd.substring(MONTH_SEGMENT);
  iD = datetimelcd.substring(DAY_SEGMENT);
  ih = datetimelcd.substring(HOUR_SEGMENT);
  im = datetimelcd.substring(MINUTE_SEGMENT);
  is = datetimelcd.substring(SECOND_SEGMENT);

  // wait for motor driver board to figure out positions, LST, etc.
  char cccc;
  serial = "";

  while(cccc != '$')
  {
    if(MOTOR_DRIVER_SERIAL.available() > 0)
    {
      cccc = MOTOR_DRIVER_SERIAL.read();
      serial += String(cccc);
    }
  }

  // process the recieved information
  serial = serial.substring(0, serial.indexOf("$"));
  // update the position, time, LST, etc variables in AstroCalcs (it takes too long to calculate them on the Mega)
  calcs.updateTimeManual(serial);
  serial = "";

  // turn off LED to show finished loading
  digitalWrite(LED_BUILTIN, LOW);

  // initial position
  calcs.setAltAz(0, 90);
  ra = calcs.getRA();
  dec = calcs.getDec();
  
  // show done screen
  lcd.clear();
  lcd.setCursor(0, 0);

  /*
  |  ANSSEN ALHENA EQ  |
  |  MOUNT CONTROLLER  |
  |        vA.B        |
  |        Done        |
  */

  //         01234567890123456789
  lcd.print("  ANSSEN ALHENA EQ  ");
  lcd.setCursor(0, 1);
  //         01234567890123456789
  lcd.print("  MOUNT CONTROLLER  ");
  lcd.setCursor(0,2);
  //         01234567890123456789
  lcd.print("        v"+VERSION+"        ");
  lcd.setCursor(0,3);
  //         01234567890123456789
  lcd.print("        Done        ");
  view = "tracking";
}


void loop()
{
  // only try to read serial if the computer is attached
  if(COMPUTER_SERIAL.available())
  {
    //serial reading
    c = COMPUTER_SERIAL.read();
    serial += String(c);

    // Get right ascention
    if(serial.endsWith("#:GR#") or serial == "#:GR#")
    {
      COMPUTER_SERIAL.print("#"+zeroflush(calcs._rah, 2)+":"+zeroflush(calcs._ram, 2)+":"+zeroflush(round(calcs._ras), 2)+"#");
      serial = "";
    }
    
    // Get declination
    if(serial.endsWith("#:GD#") or serial == "#:GD#")
    {
      int n = 1;

      if(calcs.getDec() < 0.0){
        n = -1;
      }

      String p = zeroflush(round(calcs._decs),2);
      String q = zeroflush(calcs._decm,2);

      if(p.length() != 2){
        p = "00";
      }

      if(q.length() != 2){
        q = "00";
      }

      COMPUTER_SERIAL.print("#"+signflush(calcs._decd,3,n)+"*"+q+":"+p+"#");
      serial = "";
    }

    // Change precision -- to be implemented
    if(serial == "#:U#")
    {
      precision = !precision;
      serial = "";
    }
    
    // Stop (to be implemented)
    if(serial == "#:Q#"){//Stop.
      serial = "";
    }

    // Set right ascention
    if(serial.endsWith(":Sr"))
    {
      char cc;
      
      // read until end of line
      while(cc != '#')
      {
        if(COMPUTER_SERIAL.available() > 0)
        {
          cc = COMPUTER_SERIAL.read();
          serial += String(cc);
        }
      }

      // trim serial string
      serial = serial.substring(3);

      // set ra to the serial string
      ra = (serial.substring(0, 2)).toDouble()*15.0;
      ra += (serial.substring(3, 5)).toDouble()*(1.0/60.0)*15.0;
      ra += (serial.substring(6,8)).toDouble()*(1.0/3600.0)*15.0;

      serial = "";

      // tell the computer that we got the message
      COMPUTER_SERIAL.print("1");
      delay(500);
    }

    // Set declination
    if(serial.endsWith(":Sd"))
    {
      char cc;
      int todec = 0;
      
      // read the serial
      while(cc != '#')
      {
        if(COMPUTER_SERIAL.available() > 0)
        {
          cc = COMPUTER_SERIAL.read();
          serial += String(cc);
        }
      }

      // if it is negative
      if(serial.charAt(3) == '-')
      {
        dec = (serial.substring(3,6)).toDouble();
        dec -=(serial.substring(7,9)).toDouble()/60.0;
        dec -=(serial.substring(10,12)).toDouble()/3600.0;
      }

      // if it is positive
      if(serial.charAt(3) == '+')
      {
        dec = (serial.substring(4,6)).toDouble();
        dec += (serial.substring(7,9)).toDouble()/60.0;
        dec += (serial.substring(10,12)).toDouble()/3600.0;
      }

      // tell the compute that we got the message
      COMPUTER_SERIAL.print("1");
      serial = "";
    }

    // Move
    if(serial.endsWith(":MS#"))
    {
      // tell the computer that we got the message
      COMPUTER_SERIAL.print("0");

      // tell the motor driver board the coordinates to go to
      MOTOR_DRIVER_SERIAL.print("$R|"+String(ra)+"|");
      MOTOR_DRIVER_SERIAL.print("$D|"+String(dec)+"|");

      // tell the motor driver board to move
      MOTOR_DRIVER_SERIAL.print("$M");

      serial = "";

      //clear LCD
      lcd.clear();

      // estimate the time remaining
      estTime = slewtime(calcs.getRA(), calcs.getDec(), ra, dec);

      // set the position in the AstroCalcs library
      calcs.setRADEC(ra, dec);

      // change the view for next update
      view = "slewing";
    }

    // Get distance from UTC
    if(serial.endsWith(":GG#"))
    {
      // tell the computer what time zone we are in
      COMPUTER_SERIAL.print("+10#"); //to be properly implemented -- I hate time zones
      serial = "";
    }

    // Get site longitude
    if(serial.endsWith(":Gg#"))
    {
      COMPUTER_SERIAL.print(STR_LONGITUDE + "#");
      serial = "";
    }

    // Get site latitude
    if(serial.endsWith(":Gt#"))
    {
      COMPUTER_SERIAL.print(STR_LATITUDE + "#");
      serial = "";
    }

    // Get ???
    if(serial.endsWith(ack))
    {
      COMPUTER_SERIAL.print("G");
      serial = "";
    }

    // Get product name
    if(serial.endsWith(":GVP#"))
    {
      COMPUTER_SERIAL.print("ANSSEN Alhena"); //to be properly implemented
      serial = "";
    }

    // Get product version
    if(serial.endsWith(":GVN#"))
    {
      COMPUTER_SERIAL.print(VERSION); 
      serial = "";
    }

    // Get altitude
    if(serial.endsWith(":GA#"))
    {
      // get the alt from astrocalcs library
      double x = calcs._alt;

      int deg = 0;
      double m = 0.0;
      int mins = 0;
      int sec = 0;
      int n = 0;

      // if the altitude is positive
      if(x >=0.0)
      {
        deg = floor(x);
        m = (x - deg) * 60;
        mins = floor(m);
        sec = floor((m - mins) * 60);
        n = 1;
      }

      // if the altitude is negative
      else
      {
        deg = ceil(x);
        m = (x - deg) * 60;
        mins = ceil(m);
        sec = ceil((m - mins) * 60);
        n = -1;
      }

      // tell the computer the altitude
      COMPUTER_SERIAL.print(signflush(deg,3,n)+"*"+zeroflush(mins,2)+"#");

      serial = "";
    }

    // Get azimuth
    if(serial.endsWith(":GZ#"))
    {
      // get the alt from AstroCalcs
      double x = calcs._az;

      int deg = 0;
      double m = 0.0;
      int mins = 0;
      int sec = 0;
      int n = 0;

      // convert to degrees/minutes/seconds
      deg = floor(x);
      m = (x - deg) * 60;
      mins = floor(m);
      sec = floor((m - mins) * 60);
      n = 1;

      // if negative 
      if(calcs.getDec() < 0.0){
        n = -1;
      }

      // tell the computer the azimuth
      COMPUTER_SERIAL.print(zeroflush(deg,3)+"*"+zeroflush(mins,2)+"#");
      serial = "";
    }
  }


  // Alignment
  if(digitalRead(ALIGN_PIN) == HIGH)
  {
    
    // if already not aligning
    if(aligning == false)
    {
      // show alignment screen
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Beginning alignment");
      lcd.setCursor(0,1);
      lcd.print("getting best star...");

      bool gotstar = false;

      serial = "";

      // ask the ESP to get the best alignment star from a python flask server
      ESP_SERIAL.print("$A");

      // tell the motor driver that we are aligning
      MOTOR_DRIVER_SERIAL.print("$A");

      // wait for the response from the ESP
      while(gotstar == false)
      {
        // listen on serial
        if(ESP_SERIAL.available()>0)
        {
          char cc = ESP_SERIAL.read();
          serial += String(cc);
          if(cc == '~'){
            gotstar = true;
          }
        }
      }

      // process response
      int firstI = serial.indexOf('|');
      int secondI = serial.lastIndexOf('|');

      String star = serial.substring(3, firstI);
      String starra = serial.substring(firstI+1, secondI);
      String stardec = serial.substring(secondI+1);

      // show star
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Star Chosen:");
      lcd.setCursor(0,1);
      lcd.print(star.substring(0, 20));
      lcd.setCursor(0,2);
      lcd.print("Press \"align\" button");
      lcd.setCursor(0,3);
      lcd.print("to slew to star");

      serial = "";

      // wait until button pressed
      while(digitalRead(ALIGN_PIN) != HIGH){
        delay(1);
      }

      // show slewing screen
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Slewing to star");

      // set target
      ra = starra.toDouble();
      dec = stardec.toDouble();

      // estimate time
      estTime = slewtime(calcs.getRA(), calcs.getDec(), ra, dec);

      // set target in AstroCalcs
      calcs.setRADEC(starra.toDouble(), stardec.toDouble());

      // tell the motor driver board to move
      MOTOR_DRIVER_SERIAL.print("$R|"+String(ra)+"|");
      MOTOR_DRIVER_SERIAL.print("$D|"+String(dec)+"|");
      MOTOR_DRIVER_SERIAL.print("$M");

      // change view to slewing
      view = "slewing";

      // make sure that we set the alignment next time the button is hit
      aligning = true;

      delay(1000);
    }

    // if we already chose a ster
    else
    {
      serial = "";

      // tell the motor driver board that the button was pressed
      MOTOR_DRIVER_SERIAL.print("$A");

      // show the aligned screen
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Aligned!");
      delay(1000);

      // change view
      view = "tracking";

      // make sure that next time the align button is pressed it is normal
      aligning = false;
    }
  }

  //Park
  if(digitalRead(PARK_PIN)==HIGH)
  {
    // show parked screen
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Parking...");
    lcd.setCursor(0, 1);
    lcd.print("Turn off once parked");

    // tell motor driver board that we are parked
    MOTOR_DRIVER_SERIAL.print("$P");

    // do nothing
    while(true){}
  }

  delay(1); //give space for any analogue read


  // Update screen every second
  if(millis()%1000 ==0)
  {
    // update time
    is = String(is.toInt() + 1);

    // tracking view
    if(view == "tracking"){
      /*
      |RA/DEC    ALT/AZ    |
      |00h00m00s s00*00'00"|
      |000*00:00 000*00'00"|
      |STATUS: TRACKING    |
      */
      lcd.setCursor(0, 0);
      lcd.print("RA/DEC    ALT/AZ   ");
      lcd.setCursor(0, 1);

      // get alt
      double x = calcs._alt;

      int deg = 0;
      double m = 0.0;
      int mins = 0;
      int sec = 0;
      int n = 0;

      // if positive
      if(x >=0.0){
        deg = floor(x);
        m = (x - deg) * 60;
        mins = floor(m);
        sec = floor((m - mins) * 60);
        n = 1;
      }

      // if negative
      else{
        deg = ceil(x);
        m = (x - deg) * 60;
        mins = ceil(m);
        sec = ceil((m - mins) * 60);
        n = -1;
      }

      // display position
      lcd.print(zeroflush(calcs._rah, 2)+"h"+zeroflush(calcs._ram, 2)+"m"+zeroflush(round(calcs._ras), 2)+"s "+signflush(deg,3,n)+"*"+zeroflush(mins,2)+"'"+zeroflush(sec,2)+"\"");
      lcd.setCursor(0, 2);

      // do similar with az
      x = calcs._az;
      deg = floor(x);
      m = (x - deg) * 60;
      mins = floor(m);
      sec = floor((m - mins) * 60);
      n = 1; // default to positive

      if(calcs.getDec() < 0.0){
        n = -1;
      }

      lcd.print(signflush(calcs._decd,3,n)+"*"+zeroflush(calcs._decm,2)+":"+zeroflush(round(calcs._decs), 2)+" " +zeroflush(deg,3)+"*"+zeroflush(mins,2)+"'"+zeroflush(sec,2)+"\"");
      lcd.setCursor(0,3);
      lcd.print("STATUS: TRACKING    ");
    }

    // slewing view
    if(view == "slewing")
    {
      /*
      |TARGET    CURRENT   |
      |00h00m00s 00h00m00s |
      |s00*00:00 s00*00'00"|
      |STATUS: SLEWING     |
      */
      lcd.setCursor(0, 0);
      lcd.print("TARGET    CURRENT   ");
      lcd.setCursor(0, 1);

      // display ra's
      lcd.print(zeroflush(calcs._rah,2)+"h"+zeroflush(calcs._ram,2)+"m"+zeroflush(round(calcs._ras),2)+"s 00h00m00s");
      lcd.setCursor(0,2);

      // format declination
      int n = 1;
      if(calcs.getDec() < 0.0){
        n = -1;
      }

      String p = zeroflush(round(calcs._decs),2);
      String q = zeroflush(calcs._decm,2);

      // escape errors
      if(p.length() != 2){
        p = "00";
      }
      if(q.length() != 2){
        q = "00";
      }

      // display decs
      lcd.print(signflush(calcs._decd,3,n)+"*"+q+":"+p+" +00*00:00 ");

      lcd.setCursor(0, 3);
      lcd.print("STATUS: SLEWING     ");

      // increment simulator counter
      sim++;

      // if it has been longer than the estimated time remaining, return to tracking view
      if(sim >= estTime)
      {
        sim = 0;
        view = "tracking";
        lcd.clear();
      }
    }

    // parked view
    if(view == "parked"){
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("PARKED");
    }
  }
}


/*
  zeroflush
  creates a string of an integer which is flushed with zeros to a certain length
  :: int x   :: the integer to flush
  :: int len :: the length to flush to
  returns a string of the integer flushed with zeros
*/
String zeroflush(int x, int len)
{
  // remove sign
  x = abs(x);

  // convert to string
  String res = String(x);

  // calculate amount of zeros required
  int diff = len - String(x).length();

  // if it is already long enough or too long, return string
  if(diff <= 0){
    return res;
  }

  // put zeros at the front until long enough, then return string
  else
  {
    for(int i = 0; i < diff; i++){
      res = "0"+res;
    }
    return res;
  }
}


/*
  signflush
  creates a string from an integer flushed with zeros and with a sign
  :: int x   :: the integer to convert to string
  :: int len :: the length of the resultant string
  :: int n   :: an integer denoting the sign (-1 or 1)
  returns a string of the int flushed with 0's and with a sign.
*/
String signflush(int x, int len, int n)
{
  // clearly I had a bug at some point
  len = len;

  // convert to string
  String res = String(x);

  int diff = 0;

  // calculate the amount of zeros needed for negative numbers
  if(x < 0){
    diff = len - String(x).length();
  }

  // calculate the amount of zeros needed for positive numbers
  else{
    diff = len - String(x).length()-1;
  }

  // if it is long enough or too long
  if(diff <= 0)
  {
    // if it is positive
    if(x >= 0)
    {
      if(n == -1){
        res = "-"+res;
      }
      else{
        res = "+"+res;
      }
    }
    return res;
  }

  // whack zeros on the front until it is long enough
  else
  {
    // whack zeros on
    for(int i = 0; i < diff; i++){
      res = "0"+String(abs(x));
    }

    // if it is positive
    if(x >= 0)
    {
      // sometimes I want a negative on there anyway
      if(n == -1)
      {
        res = "-"+res;
      }
      else{
        res = "+"+res;
      }
    }
    else{
      res = "-"+res;
    }
    return res;
  }
}


/*
  slewtime
  calculates the approximate time for a slew
  :: double ira  :: the initial right ascention
  :: double idec :: the initial declination
  :: double tra  :: the target right ascention
  :: double tdec :: the target declination
  returns an integer of the amount of seconds it will take to do that slew
*/
int slewtime(double ira, double idec, double tra, double tdec)
{
  // convert to radians
  ira = radians(ira);
  idec = radians(idec);
  tra = radians(tra);
  tdec = radians(tdec);

  // convert to distance around a circle
  distance = degrees(round(acos(cos(idec)*cos(tdec)*cos(tra-ira)+sin(idec)*sin(tdec))));

  // time = distance * speed (add 10 for good measure)
  return round(distance*SPEED)+10;
}
