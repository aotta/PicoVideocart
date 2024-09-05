# PicoVideocart
Flash cart for SABA Videoplay - porting of 3DMaze PicoVideocart

This is a forked version of https://github.com/ZX-80/PicoVideocart (see original project for details) for using a cheaper and simpler pcb based on pico "Purple" clone and using its 16mb flash for storing games instead of the original SDCard

Since now it was only tested on SABA Videoplay (PAL), but it should work in Fairchild Channel-F (NTSC) too.

Thanks to my friend Dave, here's some more info and the label for the shell:
https://www.nightfallcrew.com/05/09/2024/flash-multicart-for-saba-videoplay-fairchild-channel-f/


**WARNING!** "purple" Pico has not the same pinout of original Raspberry "green" ones, you MUST use the clone or you may damage your hardware.

![ScreenShot](https://raw.githubusercontent.com/aotta/PicoVideocart/main/pictures/saba.jpg)
![ScreenShot](https://raw.githubusercontent.com/aotta/PicoVideocart/main/pictures/saba1.jpg)
![ScreenShot](https://raw.githubusercontent.com/aotta/PicoVideocart/main/pictures/saba4.jpg)
![ScreenShot](https://raw.githubusercontent.com/aotta/PicoVideocart/main/pictures/saba3.jpg)
![ScreenShot](https://raw.githubusercontent.com/aotta/PicoVideocart/main/pictures/saba5.jpg)


Kicad project and gerbers files for the pcb are in the PCB folder, you need only a diode and a push buttons for resetting the cart if needed or want restart. 
Add you pico clone, and flash the firmware ".uf2" in the Pico by connecting it while pressing button on Pico and drop it in the opened windows on PC.
"Bin" files should be added in flash using Arduino IDE plugin ( https://github.com/earlephilhower/arduino-littlefs-upload )

More info on AtariAge forum: https://forums.atariage.com/topic/274416-the-official-channel-f-thread/page/56/


Even if the diode should protect your console, **DO NOT CONNECT PICO WHILE INSERTED IN A POWERED ON CONSOLE!**
