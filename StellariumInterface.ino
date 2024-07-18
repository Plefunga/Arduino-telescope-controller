/*
  Copyright (C) 2024 Nathan Carter

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  To read the full terms and conditions, see https://www.gnu.org/licenses/.
*/


#include <Wire.h>
#include <AstroCalcs.h>
#include <Position.h>
#include "dummy_motors.hpp"

// change the below to suit your specific LCD
#include <LiquidCrystal_PCF8574.h>
LiquidCrystal_PCF8574 lcd(0x27);

// change the below coordinates to your specific location
#define LONGITUDE 150.944799
#define LATITUDE -31.078821
#define STR_LONGITUDE String("150*57")
#define STR_LATITUDE String("-31*05")

AstroCalcs calcs(LONGITUDE, LATITUDE);

// change this according to your mount and hardware
#define SPEED 0.85 // in seconds per degree, for estimating how long a slew will take
#define ALIGN_PIN 6
#define PARK_PIN 7
#define CANCEL_PIN 2
#define NEXT_STAR_PIN 3
#define EXTRA_BUTTON 6
#define INPUT_MODE LOW

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

#define VERSION String("1.6.0")

// choose which alignment code to use -- online or local
//#define ALIGNMENT_CODE_ONLINE
#define ALIGNMENT_CODE_LOCAL

// setting various variables
String view = "parked";
bool parked = true;
String serial = "";
char c;
bool precision = false;
int distance = 0;
int sim = 0;
int estTime = 10; // estimated time of slew
int offset = 0;

int align_index = 0;
bool start_align = false;

String ack = ":" + String(char(6))+"#"; // used in ASCOM communication. It didn't work when I was just pasting it in the program as an ASCII character (␆)

// initialising ra and dec
double ra = 0.0;
double dec = 0.0;

// initial time
String iY, iM, iD, ih, im, is;
String strdec;
String datetime;

bool aligning = false;

bool hand_controller_attached = true;


// the dummy motors for making things more fancy without needing the entire accelstepper library
DummyMotor dummy_ra = DummyMotor();
DummyMotor dummy_dec = DummyMotor();

void setup()
{
    // initialise serial communication
    COMPUTER_SERIAL.begin(COMPUTER_SERIAL_BAUDRATE);
    ESP_SERIAL.begin(ESP_SERIAL_BAUDRATE);
    MOTOR_DRIVER_SERIAL.begin(MOTOR_DRIVER_SERIAL_BAUDRATE);

    // initialise pinModes for pins
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(ALIGN_PIN, INPUT_PULLUP);
    pinMode(PARK_PIN, INPUT_PULLUP);
    pinMode(CANCEL_PIN, INPUT_PULLUP);
    pinMode(NEXT_STAR_PIN, INPUT_PULLUP);

    dummy_ra.setAcceleration(400.0/5000.0);
    dummy_dec.setAcceleration(400.0/4000.0);
    dummy_ra.setMaxSpeed(4000.0/5000.0);
    dummy_dec.setMaxSpeed(4000.0/4000.0);

    // these are debug ones that are much faster than reality
    dummy_ra.setAcceleration(6400.0/5000.0);
    dummy_dec.setAcceleration(6400.0/4000.0);
    dummy_ra.setMaxSpeed(32000.0/5000.0);
    dummy_dec.setMaxSpeed(32000.0/4000.0);

    // initialise LCD (and Wire)
    Wire.begin();
    Wire.beginTransmission(LCD_ADDRESS);
    int e = Wire.endTransmission();

    // if LCD exists
    if (e == 0)
    {
    }

    else
    {
        hand_controller_attached = false;
        // if no LCD, flash buildin led
        for(int repeats = 0; repeats <= 4; repeats++)
        {
            digitalWrite(LED_BUILTIN, HIGH);
            delay(100);
            digitalWrite(LED_BUILTIN, LOW);
            delay(100);
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
    //         01234567   89012   3456789
    lcd.print("       v"+VERSION+"       ");
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
                        if(ccc != '&')
                        {
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
    //         01234567   89012   3456789
    lcd.print("       v"+VERSION+"       ");
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
    offset = 0;

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
    calcs.setAltAz(0, 270);
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
    //         01234567   89012   3456789
    lcd.print("       v"+VERSION+"       ");
    lcd.setCursor(0,3);
    //         01234567890123456789
    lcd.print("        Done        ");
    view = "tracking";
    parked = false;
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
            //Position p = calcs.getPosition(offset);
            int rah, ram;
            double ras;
            calcs.curr_pos.raHMS(&rah, &ram, &ras);
            char s[10];
            sprintf(s, "#%02d:%02d:%02d#", abs(rah), abs(ram), (int)abs(ras));
            COMPUTER_SERIAL.print(s);
            serial = "";
        }

        // Get declination
        if(serial.endsWith("#:GD#") or serial == "#:GD#")
        {
            //Position p = calcs.getPosition(offset);
            int decd, decm;
            double decs;
            calcs.curr_pos.decDMS(&decd, &decm, &decs);
            char s[11];
            sprintf(s, "#%+03d*%02d:%02d#", decd, abs(decm), (int)abs(decs));
            COMPUTER_SERIAL.print(s);
            serial = "";
        }

        // Change precision -- to be implemented
        if(serial == "#:U#")
        {
            precision = !precision;
            serial = "";
        }

        // Stop (to be implemented)
        if(serial == "#:Q#")
        {
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

            // new version that uses the dummy motors
            double dummy_ra_pos = calcs.getRA();
            double dummy_dec_pos = calcs.getDec();

            double ha = calcs.getHA() - 180;
            if(ha > 360)
            {
                ha -= 360;
            }
            if(ha < 0)
            {
                ha += 360;
            }

            if(ha > 180)
            {
                dummy_dec_pos = (-180) - dummy_dec_pos;
                dummy_ra_pos -= 180;
            }

            double dummy_ra_target = ra;
            double dummy_dec_target = dec;

            ha = calcs.getLST() - ra - 180;
            if(ha > 360)
            {
                ha -= 360;
            }
            if(ha < 0)
            {
                ha += 360;
            }

            if(ha > 180)
            {
                dummy_dec_target = (-180) - dummy_dec_target;
                dummy_ra_target -= 180;
            }

            while(dummy_ra_pos > 360){dummy_ra_pos -= 360;}
            while(dummy_ra_pos < 0){dummy_ra_pos += 360;}
            while(dummy_ra_target > 360){dummy_ra_target -= 360;}
            while(dummy_ra_target < 0){dummy_ra_target += 360;}

            dummy_ra.setPosition(dummy_ra_pos);
            dummy_dec.setPosition(dummy_dec_pos);
            dummy_ra.moveTo(dummy_ra_target);
            dummy_dec.moveTo(dummy_dec_target);

            // set the position in the AstroCalcs library
            //

            // change the view for next update
            view = "slewing";
        }

        // Get distance from UTC
        if(serial.endsWith(":GG#"))
        {
            // tell the computer what time zone we are in
            COMPUTER_SERIAL.print("+10#"); // to be properly implemented -- I hate time zones
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
            COMPUTER_SERIAL.print("ANSSEN Alhena"); // to be properly implemented
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
            //Position p = calcs.getPosition(offset);

            int deg, min;
            double sec;

            calcs.curr_pos.altDMS(&deg, &min, &sec);

            char s[7];
            sprintf(s, "%+03d*%02d#", deg, abs(min));

            // tell the computer the altitude
            COMPUTER_SERIAL.print(s);

            serial = "";
        }

        // Get azimuth
        if(serial.endsWith(":GZ#"))
        {
            // get the alt from AstroCalcs
            //Position p = calcs.getPosition(offset);

            int deg, min;
            double sec;

            calcs.curr_pos.azDMS(&deg, &min, &sec);

            char s[7];
            sprintf(s, "%03d*%02d#", abs(deg), abs(min));

            // tell the computer the azimuth
            COMPUTER_SERIAL.print(s);
            serial = "";
        }
    }


    // Alignment
    if((digitalRead(ALIGN_PIN) == INPUT_MODE || start_align == true) && hand_controller_attached)
    {

        // if already not aligning
        if(aligning == false)
        {
            start_align = false;
            bool exit_align = false;

            // if we pressed the button, reset align index
            if(!start_align)
            {
              //align_index = 0;
            }
            
            // show alignment screen
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Beginning alignment");
            lcd.setCursor(0,1);
            lcd.print("getting best star...");

            bool gotstar = false;

            serial = "";

            // ask the ESP to get the best alignment star from a python flask server
            #ifdef ALIGNMENT_CODE_ONLINE
            ESP_SERIAL.print("$A" + String(align_index) + "|");
            // tell the motor driver that we are aligning
            MOTOR_DRIVER_SERIAL.print("$A|");
            #endif

            #ifdef ALIGNMENT_CODE_LOCAL
            MOTOR_DRIVER_SERIAL.print("$A" + String(align_index) + "|");
            #endif

            

            // wait for the response from the ESP
            digitalWrite(LED_BUILTIN, HIGH);
            while(gotstar == false)
            {
                #ifdef ALIGNMENT_CODE_ONLINE
                // listen on serial
                if(ESP_SERIAL.available() > 0)
                {
                    char cc = ESP_SERIAL.read();
                    serial += String(cc);
                    if(cc == '~'){
                        gotstar = true;
                    }
                }
                #endif
                
                #ifdef ALIGNMENT_CODE_LOCAL
                // listen on serial
                if(MOTOR_DRIVER_SERIAL.available() > 0)
                {
                    char cc = MOTOR_DRIVER_SERIAL.read();
                    serial += String(cc);
                    if(cc == '~'){
                        gotstar = true;
                    }
                }
                #endif
            }
            

            digitalWrite(LED_BUILTIN, LOW);

            // process response
            int firstI = serial.indexOf('|');
            int secondI = serial.lastIndexOf('|');
            int hash = serial.indexOf('#');

            String star = serial.substring(hash+1, firstI);
            String starra = serial.substring(firstI+1, secondI);
            String stardec = serial.substring(secondI+1);

            // show star
            lcd.clear();
            lcd.setCursor(0,0);
            lcd.print("Press \"align\" button");
            lcd.setCursor(0,1);
            lcd.print("to slew to " + star.substring(0, 9));
            lcd.setCursor(0,2);
            lcd.print("Or press \"next\" to");
            lcd.setCursor(0,3);
            lcd.print("find another star");

            serial = "";

            // wait until button pressed
            while((digitalRead(ALIGN_PIN) != INPUT_MODE) && exit_align == false)
            {
                if (digitalRead(NEXT_STAR_PIN) == INPUT_MODE)
                {
                    align_index++;
                    exit_align = true;
                    start_align = true;
                }
                if (digitalRead(CANCEL_PIN) == INPUT_MODE)
                {
                    exit_align = true;
                    view = "tracking";
                }
                delay(10);
            }

            if(exit_align == false)
            {
                // show slewing screen
                lcd.clear();
                lcd.setCursor(0, 0);
                lcd.print("Slewing to star");
    
                // set target
                ra = starra.toDouble();
                dec = stardec.toDouble();
    
                // estimate time
                estTime = slewtime(calcs.getRA(), calcs.getDec(), ra, dec);

                // new version that uses the dummy motors
                double dummy_ra_pos = calcs.getRA();
                double dummy_dec_pos = calcs.getDec();

                double ha = calcs.getHA() - 180;
                if(ha > 360)
                {
                    ha -= 360;
                }
                if(ha < 0)
                {
                    ha += 360;
                }

                if(ha > 180)
                {
                    dummy_dec_pos = (-180) - dummy_dec_pos;
                    dummy_ra_pos -= 180;
                }

                double dummy_ra_target = ra;
                double dummy_dec_target = dec;

                ha = calcs.getLST() - ra - 180;
                if(ha > 360)
                {
                    ha -= 360;
                }
                if(ha < 0)
                {
                    ha += 360;
                }

                if(ha > 180)
                {
                    dummy_dec_target = (-180) - dummy_dec_target;
                    dummy_ra_target -= 180;
                }

                while(dummy_ra_pos > 360){dummy_ra_pos -= 360;}
                while(dummy_ra_pos < 0){dummy_ra_pos += 360;}

                while(dummy_ra_target > 360){dummy_ra_target -= 360;}
                while(dummy_ra_target < 0){dummy_ra_target += 360;}

                dummy_ra.setPosition(dummy_ra_pos);
                dummy_dec.setPosition(dummy_dec_pos);
                dummy_ra.moveTo(dummy_ra_target);
                dummy_dec.moveTo(dummy_dec_target);
    
                // set target in AstroCalcs
                //calcs.setRADEC(starra.toDouble(), stardec.toDouble());
    
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
        }

        // if we already chose a star
        else
        {
            serial = "";

            // tell the motor driver board that the button was pressed
            MOTOR_DRIVER_SERIAL.print("$A|");

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
    if((digitalRead(PARK_PIN) == INPUT_MODE) && hand_controller_attached)
    {
        if(!parked)
        {
            // show parked screen
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Parking...");

            // tell motor driver board that we are parked
            MOTOR_DRIVER_SERIAL.print("$P");
            parked = true;
            view = "parking";

            double dummy_ra_pos = calcs.getRA();
            double dummy_dec_pos = calcs.getDec();

            double ha = calcs.getHA() - 180;
            if(ha > 360)
            {
                ha -= 360;
            }
            if(ha < 0)
            {
                ha += 360;
            }

            if(ha > 180)
            {
                dummy_dec_pos = (-180) - dummy_dec_pos;
                dummy_ra_pos -= 180;
            }


            while(dummy_ra_pos > 360){dummy_ra_pos -= 360;}
            while(dummy_ra_pos < 0){dummy_ra_pos += 360;}
            dummy_ra.setPosition(dummy_ra_pos);
            dummy_dec.setPosition(dummy_dec_pos);

            // initial position
            calcs.setAltAz(0, 270);
            ra = calcs.getRA();
            dec = calcs.getDec();

            // estimate time
            estTime = slewtime(dummy_ra.getPosition(), dummy_dec.getPosition(), ra, dec);

            // new version that uses the dummy motors
            double dummy_ra_target = ra;
            double dummy_dec_target = dec;

            ha = calcs.getLST() - ra - 180;
            if(ha > 360)
            {
                ha -= 360;
            }
            if(ha < 0)
            {
                ha += 360;
            }

            if(ha > 180)
            {
                dummy_dec_target = (-180) - dummy_dec_target;
                dummy_ra_target -= 180;
            }

            while(dummy_ra_target > 360){dummy_ra_target -= 360;}
            while(dummy_ra_target < 0){dummy_ra_target += 360;}
            dummy_ra.moveTo(dummy_ra_target);
            dummy_dec.moveTo(dummy_dec_target);

            // unrestrict for setting current position
            if(dummy_dec_pos < -90)
            {
              dummy_dec_pos = (-180) - dummy_dec_pos;
              dummy_ra_pos += 180;
            }
            calcs.setRADEC(dummy_ra_pos, dummy_dec_pos);

            delay(1000);
        }
        else if(view != "parking")
        {
            parked = false;
            view = "tracking";
            MOTOR_DRIVER_SERIAL.print("$P");
            lcd.clear();
            delay(1000);
        }
        

        // do nothing (or restart if button pressed)
        
        /*while(parked)
        {
            if(digitalRead(PARK_PIN) == INPUT_MODE)
            {
                parked = false;
                view = "tracking";
                MOTOR_DRIVER_SERIAL.print("$P");
            }
            delay(100);
        }*/
    }

    if(millis() % 10 == 0)
    {
        dummy_ra.run();
        dummy_dec.run();
    }
    

    // Update screen every second
    if(millis()%1000 == 0)
    {
        // update time
        is = String(is.toInt() + 1);
        offset++;
        calcs.curr_pos.increment(1);

        // tracking view
        if(view == "tracking")
        {
            /*
            |RA/DEC    ALT/AZ    |
            |00h00m00s s00*00'00"|
            |s00*00:00 000*00'00"|
            |STATUS: TRACKING    |
            */
            //lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("RA/DEC    ALT/AZ    ");
            lcd.setCursor(0, 1);

            // new idea using sprintf and position class

            char buffer[20];

            //Position p = calcs.getPosition(offset);
            
            int ldeg, lmin, rdeg, rmin;
            double lsec, rsec;

            Position p = calcs.precess_curr_pos();

            p.raHMS(&ldeg, &lmin, &lsec);
            p.altDMS(&rdeg, &rmin, &rsec);

            //calcs.curr_pos.raHMS(&ldeg, &lmin, &lsec);
            //calcs.curr_pos.altDMS(&rdeg, &rmin, &rsec);

            //sprintf(buffer, "%02dh%02dm%02ds %+03d*%02d'%02d\"", abs(p.rah), abs(p.ram), (int)(abs(p.ras)), (int)p.altd, abs(p.altm), (int)(abs(p.alts)));
            sprintf(buffer, "%02dh%02dm%02ds %+03d*%02d'%02d\"", abs(ldeg), abs(lmin), (int)(abs(lsec)), (int)rdeg, abs(rmin), (int)(abs(rsec)));
            lcd.print(buffer);

            p.decDMS(&ldeg, &lmin, &lsec);
            p.azDMS(&rdeg, &rmin, &rsec);

            //calcs.curr_pos.decDMS(&ldeg, &lmin, &lsec);
            //calcs.curr_pos.azDMS(&rdeg, &rmin, &rsec);

            lcd.setCursor(0,2);
            //sprintf(buffer, "%+03d*%02d:%02d %03d*%02d'%02d\"", p.decd, abs(p.decm), (int)(abs(p.decs)), p.azd, abs(p.azm), (int)(abs(p.azs)));
            sprintf(buffer, "%+03d*%02d:%02d %03d*%02d'%02d\"", ldeg, abs(lmin), (int)(abs(lsec)), rdeg, abs(rmin), (int)(abs(rsec)));
            lcd.print(buffer);

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

            // try new idea using sprintf

            char buffer[20];

            calcs.setRADEC(ra, dec);

            int trdeg, trmin, crdeg, crmin;
            double trsec, crsec;

            int tddeg, tdmin, cddeg, cdmin;
            double tdsec, cdsec;

            calcs.curr_pos.raHMS(&trdeg, &trmin, &trsec);
            calcs.curr_pos.decDMS(&tddeg, &tdmin, &tdsec);

            //Position p = calcs.getPosition(offset);

            double dummy_ra_pos = dummy_ra.getPosition();
            double dummy_dec_pos = dummy_dec.getPosition();

            if(dummy_dec_pos < -90)
            {
              dummy_dec_pos = (-180) - dummy_dec_pos;
              dummy_ra_pos += 180;
            }
            calcs.setRADEC(dummy_ra_pos, dummy_dec_pos);

            calcs.curr_pos.raHMS(&crdeg, &crmin, &crsec);
            calcs.curr_pos.decDMS(&cddeg, &cdmin, &cdsec);

            // sprintf(buffer, "%02dh%02dm%02ds 00h00m00s", abs(p.rah), abs(p.ram), (int)(abs(p.ras)));
            sprintf(buffer, "%02dh%02dm%02ds %02dh%02dm%02ds", abs(trdeg), abs(trmin), (int)(abs(trsec)), abs(crdeg), abs(crmin), (int)(abs(crsec)));

            lcd.print(buffer);

            // display ra's
            lcd.setCursor(0,2);

            //sprintf(buffer, "%+03d*%02d:%02d +00*00:00", p.decd, abs(p.decm), (int)abs(p.decs));
            sprintf(buffer, "%+03d*%02d:%02d %+03d*%02d:%02d", tddeg, abs(tdmin), (int)abs(tdsec), cddeg, abs(cdmin), (int)abs(cdsec));
            lcd.print(buffer);

            lcd.setCursor(0, 3);
            lcd.print("STATUS: SLEWING     ");

            // increment simulator counter
            sim++;

            // if it has been longer than the estimated time remaining, return to tracking view
            if(/*sim >= estTime || */(!dummy_ra.isMoving() && !dummy_dec.isMoving()))
            {
                sim = 0;
                view = "tracking";
                //calcs.setRADEC(ra, dec);
                lcd.clear();
            }
        }

        if(view == "parking")
        {
            /*
            |TARGET    CURRENT   |
            |00h00m00s 00h00m00s |
            |s00*00:00 s00*00'00"|
            |STATUS: PARKING     |
            */
            lcd.setCursor(0, 0);
            lcd.print("TARGET    CURRENT   ");
            lcd.setCursor(0, 1);

            // try new idea using sprintf

            char buffer[20];

            calcs.setRADEC(ra, dec);

            int trdeg, trmin, crdeg, crmin;
            double trsec, crsec;

            int tddeg, tdmin, cddeg, cdmin;
            double tdsec, cdsec;

            calcs.curr_pos.raHMS(&trdeg, &trmin, &trsec);
            calcs.curr_pos.decDMS(&tddeg, &tdmin, &tdsec);

            //Position p = calcs.getPosition(offset);

            double dummy_ra_pos = dummy_ra.getPosition();
            double dummy_dec_pos = dummy_dec.getPosition();

            if(dummy_dec_pos < -90)
            {
              dummy_dec_pos = (-180) - dummy_dec_pos;
              dummy_ra_pos += 180;
            }
            calcs.setRADEC(dummy_ra_pos, dummy_dec_pos);

            calcs.curr_pos.raHMS(&crdeg, &crmin, &crsec);
            calcs.curr_pos.decDMS(&cddeg, &cdmin, &cdsec);

            // sprintf(buffer, "%02dh%02dm%02ds 00h00m00s", abs(p.rah), abs(p.ram), (int)(abs(p.ras)));
            sprintf(buffer, "%02dh%02dm%02ds %02dh%02dm%02ds", abs(trdeg), abs(trmin), (int)(abs(trsec)), abs(crdeg), abs(crmin), (int)(abs(crsec)));

            lcd.print(buffer);

            // display ra's
            lcd.setCursor(0,2);

            //sprintf(buffer, "%+03d*%02d:%02d +00*00:00", p.decd, abs(p.decm), (int)abs(p.decs));
            sprintf(buffer, "%+03d*%02d:%02d %+03d*%02d:%02d", tddeg, abs(tdmin), (int)abs(tdsec), cddeg, abs(cdmin), (int)abs(cdsec));
            lcd.print(buffer);

            lcd.setCursor(0, 3);
            lcd.print("STATUS: PARKING     ");

            // increment simulator counter
            sim++;

            // if it has been longer than the estimated time remaining, return to tracking view
            if(/*sim >= estTime || */(!dummy_ra.isMoving() && !dummy_dec.isMoving()))
            {
                sim = 0;
                view = "parked";
                //calcs.setRADEC(ra, dec);
                lcd.clear();
            }
        }

        // parked view
        if(view == "parked")
        {
            //         01234567890123456789
            lcd.print("  ANSSEN ALHENA EQ  ");
            lcd.setCursor(0, 1);
            //         01234567890123456789
            lcd.print("  MOUNT CONTROLLER  ");
            lcd.setCursor(0,2);
            //         01234567   89012   3456789
            lcd.print("       v"+VERSION+"       ");
            lcd.setCursor(0,3);
            //         01234567890123456789
            lcd.print("       Parked       ");
        }
    }
}

/**
 * @brief calculates the approximate time for a slew
 * @param ira the initial right ascention
 * @param idec the initial declination
 * @param tra the target right ascention
 * @param tdec the target declination
 * @returns the amount of seconds that it will take to do that slew, using the SPEED macro, with 30 seconds added as this should only be used as a failsafe
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

  // time = distance * speed (add 30 to be super confident that we aren't cutting it short)
  return round(distance*SPEED)+30;
}
