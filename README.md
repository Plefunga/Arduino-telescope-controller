# Arduino-telescope-controller
Stellarium and other astronomy software compatible telescope mount controller

Currently in development - many issues at the moment.

The conroller is designed to work with three arduino boards. A Teensy 4.0 runs the motors, and an Arduino Mega runs everything that requires delay, so that the motors can run as fast as possible when slewing. The Mega runs a screen, interfaces with the computer over Serial, and communicates with an ESP8266 to get the time. The third board is the ESP8266, which uses the EzTime library to get the current UTC time.

So far, I have incorporated Stellarium communication using the LX200 protocol, which is used by Meade telescopes.

The mount that I am making this for is an Anssen Alhena mount, which will have a large 16" Newtonian on it.

TODO:
- Seems to not have the correct starting position set in the declination axis. Either do a software fix, or physically make the starting position 0. Software fix would be better.
- Try the NexStar protocol, as the LX200 seems to wait for around 13 seconds before starting.
- Add stop button
- Make a UI on the screen [in dev at the moment]
- Implement a polar align [in dev at the moment]
- Add rotary encoder support
- Implement autoguider support
- Add a park function
- Add an RTC for when there is no wifi
- For this repo: more detail and also add some photos and details of how to change it for different mounts.

There are undoubtably many more things that I need to do that I will find out as I complete these.

Currently I am having an issue with the motors on the mount that I am making this for (they are quite old).

My local astronomy club, who owns the mount that I am creating the controller for, has also asked me to create another for a historic 36" F/1 telescope (if they manage to restore it). For that, I intend to use a Raspberry Pi Pico board, as those are dual core and have wifi, so I only will use 1 board instead of 3, which simplifies it a lot.
