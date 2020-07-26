# fooztimer2

Device to alert when a game of fuzball / table soccer is gonna end.
Press the big red button to start, and you will be alerted by a
car horn when the game should stop.

The device is build on a pic1651459 from microchip, a bobby-dazzler
with 8k program storage and a whopping 1k of work ram.

![alt text][fuzboard2]

[fuzboard2]: ./images/foozboard2.png "Veroboard 1 off fooztimer"

[mp3module]: ./images/mp3module-desc.png "Veroboard 1 off fooztimer"

[bigredbutton]: ./images/big_red-desc.png "Big red button with red LED"


![alt text][mp3module]


![alt text][bigredbutton]


## BOM

* 1 x PIC16F1459
* 1 x MP3Module (from ebay)
* Big Red Button, with integrated red LED
* 3 x MOSFET IRLB8721 (overkill)
* 1 x random speaker
* 1 x 10KOhm Potentiometer
* 1 x 100uf 16v electrolytic capacitor,
      for reservoir on the 5v line, to cope with the MP3 modules
      insane spike when its turned on. 
* 1x MCP1702 3v3 LDO Voltage Regulator
* 2x 1uf 10v ceramic caps. for voltage regulator
* 4x 4.7KOhm resistors
* 1x Wooden enclosure from old speaker
* 1x stripeboard / Veroboard
* 1x pinheader


## Code

Code is written in MPLAB X IDE, with xc8 compiler v 2.10.
Its in a single c file with no external libraries used.

[code in newmain.c](./newmain.c)

## Schematics

None atm. since this is a one off project, can make if enough
request it, in kicad.
